// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoginUIWidget.generated.h"

/**
 * 
 */
UCLASS()
class MYPROJECT_API ULoginUIWidget : public UUserWidget
{
	GENERATED_BODY()
	
	public:
		virtual void NativeConstruct() override;

		UFUNCTION()
			void OnLoginClicked();

		UFUNCTION()
			void OnLogoutClicked();

		UFUNCTION()
			void ExchangeAuthCodeForTokens(const FString& Code);

		// Change login status to true/false
		void SetLoginState(bool bNewLoginState);

		void UpdateUI();

	protected:
		// UI Elements
		UPROPERTY(meta = (BindWidget))
			class UButton* LoginButton;

		UPROPERTY(meta = (BindWidget))
			class UButton* LogoutButton;

		UPROPERTY(meta = (BindWidget))
			class UTextBlock* StatusText;

		// Login state
		UPROPERTY(BlueprintReadOnly, Category = "Login")
		bool bIsLoggedIn = false;

		// Descope OIDC Parameters
		FString ClientId;
		FString RedirectUri;
		FString RedirectLogoutUri;
		
		// Descope endpoint URLs
		FString AuthUrl;
		FString TokenUrl;
		FString LogoutUrl;

		// PKCE values
		FString CodeVerifier;
		FString CodeChallenge;
		FString State;

		FString CapturedAuthCode;

		// Tokens
		FString IdToken;
		FString AccessToken;

		// Helper functions
		FString BuildLoginUrl() const;

		void ListenForAuthCodeInBackground();

		// Fetching user data variables and functions
		FString UserEmail;
		FString UserName;

		UFUNCTION()
			void FetchUserProfile(const FString& Token);

		void UpdateUIBasedOnAuth();

		// Logout helper function
		void ListenForLogout();
};
