# Installation Guide for EDU Emulator on Windows

## Prerequisites
Before you start, ensure that you have the following dependencies installed:

1. **Java Development Kit (JDK)**
   - Download the JDK from the [Oracle website](https://www.oracle.com/java/technologies/javase-jdk11-downloads.html) or use a package manager like [Chocolatey](https://chocolatey.org/).
   - Ensure you set the `JAVA_HOME` environment variable.

2. **Gradle**
   - Download Gradle from the [Gradle website](https://gradle.org/install/).
   - Add Gradle to your `PATH` environment variable.

3. **Android Studio**
   - Download and install [Android Studio](https://developer.android.com/studio).
   - Make sure to install the Android SDK and create an Android Virtual Device (AVD).


## Step-by-Step Installation

### Step 1: Clone the Repository
Open your terminal (Command Prompt or PowerShell) and run:
```bash
git clone https://github.com/Eddyaditya/android-emulator.git
cd android-emulator
```

### Step 2: Build the Project
To build the project using Gradle, run:
```bash
gradle build
```
This command compiles the project and installs dependencies.

### Step 3: Set up Android Virtual Device
1. Open Android Studio.
2. Navigate to `Tools` → `AVD Manager`.
3. Create a new AVD (e.g., Pixel 4).
4. Set the device configuration according to your needs.
5. Start the AVD to ensure it starts correctly.

### Step 4: Run the Emulator
Once everything is set up, you can run the emulator with the command:
```bash
./gradlew emulator
```
This should launch the EDU Emulator on your newly created AVD.

### Troubleshooting
If you experience issues, ensure that:
- All your dependencies are properly installed.
- Your `JAVA_HOME` and `ANDROID_HOME` environment variables are correctly set.

### Conclusion
Follow these steps carefully to have the EDU Emulator up and running on your Windows machine. Happy coding!