# Unreal Engine + Descope OAuth2 Authentication Demo

This project demonstrates how to integrate Descope's OAuth2 authentication flow into an Unreal Engine 5 game using C++. It covers the full login/logout flow, including redirecting users to the browser, receiving the authorization code via a local socket listener, exchanging it for tokens, and updating the UI accordingly.

## 📋 Prerequisites

Before using the provided C++ files, follow these setup steps inside the Unreal Editor.

## ⚙️ Setup

### 1. Set Up a New Project in Unreal Editor

- Launch the Unreal Engine Editor
- Create a new **Blank C++ Project**
- Name the project `MyProject` (recommended to match code snippets and file structure)

![Unreal Engine Create a New Project](https://i.imgur.com/JXfg9Gy.png)

### 2. Create a Blueprint Widget (UMG)

- Go to **Content Browser → Add New → User Interface → Widget Blueprint**
- Name it `WBP_LoginUI`

#### Add UI Elements:

- **Two Buttons**:
  - `LoginButton`
  - `LogoutButton`
- **One Text Block**:
  - `StatusText`

![Create Blueprint Widget](https://i.imgur.com/DySCekt.png)

#### Mark UI elements as variables:

- Open the widget in **Designer**
- Select each button and the text field
- In the **Details panel**, check **Is Variable**

**Save** the widget before continuing.

### 3. Create a C++ Class Based on `UUserWidget`

- Go to **File > New C++ Class**
- Click **Show All Classes**
- Search for and select `UserWidget`
- Click **Next**
- Name the class `LoginUIWidget`
- Click **Create Class**

### 4. Reparent the Blueprint to Your C++ Widget Class

- Open `WBP_LoginUI`
- Go to **File > Reparent Widget**
- Select your newly created `LoginUIWidget` class

![Reparent Widget](https://i.imgur.com/4N1r50U.png)

This connects your UI to the C++ backend logic.

### 5. Paste the Code

Now that your project and UI are ready:

1. Open `LoginUIWidget.h`, `LoginUIWidget.cpp`, and `MyProject.Build.cs`
2. Paste the corresponding content from this repository into each file
3. Save all files and compile the project from Unreal or Visual Studio

### 6. Run the Project

Once compiled:

- Press **Play**
- You should see your login UI with buttons and status text
- Click **Login** to launch the Descope OAuth flow
- Upon successful authentication, the game will capture the code and log the user in
- Click **Logout** to end the session

## 🛡️ About the Listener

This project includes a simple TCP socket listener to receive the redirect from Descope’s OAuth server. For production use, we recommend extracting it into a dedicated class or replacing it with a minimal Node.js or Go server if needed.