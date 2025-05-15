// Fill out your copyright notice in the Description page of Project Settings.

#include "LoginUIWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformProcess.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"

#include "Async/Async.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Networking.h"
#include "IPAddress.h"
#include "Regex.h"

#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void ULoginUIWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LoginButton)
		LoginButton->OnClicked.AddDynamic(this, &ULoginUIWidget::OnLoginClicked);

	if (LogoutButton)
		LogoutButton->OnClicked.AddDynamic(this, &ULoginUIWidget::OnLogoutClicked);

	// Set default OIDC values (replace with actual client ID)
	ClientId = TEXT("your-client-id-from-descope");
	RedirectUri = TEXT("http://localhost:3000/callback");
	RedirectLogoutUri = TEXT("http://localhost:3000/logout-callback");

	AuthUrl = TEXT("https://api.descope.com/oauth2/v1/authorize");
	TokenUrl = TEXT("https://api.descope.com/oauth2/v1/token");
	LogoutUrl = TEXT("https://api.descope.com/oauth2/v1/logout");

	bIsLoggedIn = false;
	IdToken = TEXT("");

	// Static PKCE values for now (replace or generate dynamically)
	CodeVerifier = TEXT("N7QLWqAp2aYxDGRtqbXjbfusYLE97XAui-nnW9hOofI");
	CodeChallenge = TEXT("KM6LN2hMVS5xeS3CHhoNuHtqtWD0-EIbkIq8u_KZl3U");
	State = TEXT("KM6LN2fFDS32duHtqtWD0-EIbkIq8u_KfG3U");

	UpdateUI();
}

void ULoginUIWidget::OnLoginClicked()
{
	// Construct URL for the login endpoint
	FString LoginURL = BuildLoginUrl();
	UE_LOG(LogTemp, Log, TEXT("Launching login URL: %s"), *LoginURL);

	// Open the constructed URL in a browser
	FPlatformProcess::LaunchURL(*LoginURL, nullptr, nullptr);

	// Start HTTP listener in background
	ListenForAuthCodeInBackground();

	SetLoginState(true);
}

void ULoginUIWidget::OnLogoutClicked()
{
	UE_LOG(LogTemp, Log, TEXT("Logout button pressed!"));

	if (IdToken.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("No ID token available for logout."));
		return;
	}

	FString LogoutUrl = FString::Printf(
		TEXT("https://api.descope.com/oauth2/v1/logout?")
		TEXT("id_token_hint=%s&post_logout_redirect_uri=%s"),
		*FGenericPlatformHttp::UrlEncode(IdToken),
		*FGenericPlatformHttp::UrlEncode(RedirectLogoutUri)
	);

	FPlatformProcess::LaunchURL(*LogoutUrl, nullptr, nullptr);
	ListenForLogout();
}

FString ULoginUIWidget::BuildLoginUrl() const
{
	FString URL = FString::Printf(
		TEXT("%s?response_type=code&client_id=%s&redirect_uri=%s&code_challenge=%s&code_challenge_method=S256&state=%s&scope=openid%%20email%%20profile"),
		*AuthUrl,
		*ClientId,
		*FGenericPlatformHttp::UrlEncode(RedirectUri),
		*CodeChallenge,
		*State
	);

	return URL;
}

// Updates the internal login state and refreshes the UI
void ULoginUIWidget::SetLoginState(bool bNewLoginState)
{
	bIsLoggedIn = bNewLoginState;
	UpdateUI();
}

// Refreshes the UI based on the current login state
void ULoginUIWidget::UpdateUI()
{
	// Update the status text to reflect login state
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(bIsLoggedIn ? TEXT("Logged in") : TEXT("Not logged in")));
	}

	// Show or hide the login button based on state
	if (LoginButton)
	{
		LoginButton->SetVisibility(bIsLoggedIn ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	// Show or hide the logout button based on state
	if (LogoutButton)
	{
		LogoutButton->SetVisibility(bIsLoggedIn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void ULoginUIWidget::ListenForAuthCodeInBackground()
{
	// Start a background thread using a lambda
	TFunction<void()> Task = [this]()
	{
		// Create a new socket to listen for incoming connections
		FSocket* ListenerSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("AuthSocket"), false);

		// Setup the IP address and port as a InternetAdress object
		TSharedRef<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		bool bIsValid;
		Addr->SetIp(TEXT("127.0.0.1"), bIsValid);
		Addr->SetPort(3000);

		// Bind and listen on the port
		bool bBind = ListenerSocket->Bind(*Addr);
		bool bListen = ListenerSocket->Listen(1);

		if (!bBind || !bListen)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to bind or listen on port 3000"));
			return;
		}

		// Debugging log
		UE_LOG(LogTemp, Log, TEXT("Listening for Descope redirect on http://localhost:3000/callback"));

		// Wait for an incoming HTTP connection
		FSocket* ConnectionSocket = nullptr;
		while (!ConnectionSocket)
		{
			ConnectionSocket = ListenerSocket->Accept(TEXT("IncomingConnection"));
		}

		// Receive HTTP request
		TArray<uint8> Data;
		uint32 DataSize;
		FString RequestString;

		while (ConnectionSocket->HasPendingData(DataSize))
		{
			Data.SetNumUninitialized(DataSize);
			int32 BytesRead = 0;
			ConnectionSocket->Recv(Data.GetData(), DataSize, BytesRead);

			RequestString += FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(Data.GetData())));
		}

		// Extract code from URL
		FString Code;
		FRegexPattern Pattern(TEXT("GET /callback\\?code=([^& ]+)"));
		FRegexMatcher Matcher(Pattern, RequestString);

		if (Matcher.FindNext())
		{
			Code = Matcher.GetCaptureGroup(1);
			CapturedAuthCode = Code;
			UE_LOG(LogTemp, Log, TEXT("Captured Auth Code: %s"), *Code);
		}

		// Send HTTP response
		FString Response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
			"<html><body><h2>Login successful. You can return to the game.</h2></body></html>";

		auto Ansi = StringCast<ANSICHAR>(*Response);
		int32 Sent;
		ConnectionSocket->Send((uint8*)Ansi.Get(), Ansi.Length(), Sent);

		ConnectionSocket->Close();
		ListenerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket);
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket);

		// Back to game thread (optional: notify UI)
		AsyncTask(ENamedThreads::GameThread, [this, Code]()
		{
			if (!Code.IsEmpty())
			{
				SetLoginState(true); // Update UI state
				// Exchange code for token here
				ExchangeAuthCodeForTokens(Code);
			}
		});
	};

	::Async(EAsyncExecution::Thread, Task);
}

// Exchanges the authorization code for access, ID, and refresh tokens using Descope's /token endpoint
void ULoginUIWidget::ExchangeAuthCodeForTokens(const FString& Code)
{
	// Create an HTTP POST request
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TokenUrl);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));

	// Format POST body with required OAuth2 parameters
	FString PostData = FString::Printf(
		TEXT("grant_type=authorization_code&code=%s&redirect_uri=%s&client_id=%s&code_verifier=%s"),
		*FGenericPlatformHttp::UrlEncode(Code),
		*FGenericPlatformHttp::UrlEncode(RedirectUri),
		*FGenericPlatformHttp::UrlEncode(ClientId),
		*FGenericPlatformHttp::UrlEncode(CodeVerifier)
	);

	// Attach the POST data to the request
	Request->SetContentAsString(PostData);

	// Set up the callback to handle the response
	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		// Check if the request failed or response is invalid
		if (!bSuccess || !Response.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Token exchange request failed."));
			return;
		}

		// Get the JSON response as a string
		FString JsonStr = Response->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);

		// Parse the JSON into a usable object
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to parse token JSON response."));
			return;
		}

		// Extract tokens from the JSON response
		FString AccessToken = JsonObject->GetStringField("access_token");
		FString RetrievedIdToken = JsonObject->GetStringField("id_token");
		FString RefreshToken;
		JsonObject->TryGetStringField("refresh_token", RefreshToken);

		// Save the ID token for logout or session checks
		IdToken = RetrievedIdToken;

		// Log all tokens to Output Log (for debugging)
		UE_LOG(LogTemp, Log, TEXT("Access Token: %s"), *AccessToken);
		UE_LOG(LogTemp, Log, TEXT("ID Token: %s"), *RetrievedIdToken);
		UE_LOG(LogTemp, Log, TEXT("Refresh Token: %s"), *RefreshToken);

		// Optional: update UI or state
		SetLoginState(true);
	});

	// Send the request
	Request->ProcessRequest();
}

// Fetches the authenticated user's profile info from Descope using the access token
void ULoginUIWidget::FetchUserProfile(const FString& Token)
{
	// Create a new HTTP GET request using Unreal's HTTP module
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();

	// Set the URL to Descope's /userinfo endpoint
	Request->SetURL(TEXT("https://api.descope.com/oauth2/v1/userinfo"));
	
	// Set HTTP method to GET
	Request->SetVerb(TEXT("GET"));

	// Add the Authorization header using the Bearer token
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));

	// Set up the callback that will run when the HTTP request completes
	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
	{
		// Check if the request failed or the response is invalid
		if (!bSuccess || !Resp.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to fetch user info."));
			return;
		}

		// Extract the response content as a JSON string
		FString JsonStr = Resp->GetContentAsString();

		// Prepare to deserialize the JSON string into a JSON object
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);

		// Try to parse the JSON
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			// Extract the user's email and name from the JSON object
			UserEmail = JsonObject->GetStringField("email");
			UserName = JsonObject->GetStringField("name");

			// Mark the user as logged in
			bIsLoggedIn = true;

			// Log the result to the output log for debugging
			UE_LOG(LogTemp, Log, TEXT("User Info: %s, %s"), *UserName, *UserEmail);

			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				if (StatusText)
				{
					StatusText->SetText(FText::FromString(
						FString::Printf(TEXT("Logged in Username: %s Email: %s"), *UserName, *UserEmail)
					));
				}

				UpdateUI();
			});
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to parse user info JSON."));
		}
	});

	Request->ProcessRequest();
}
