plugins {
    id("com.android.application")
}

android {
    namespace = "cz.voxelcraft"
    compileSdk = 36
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "cz.voxelcraft"
        minSdk = 30
        targetSdk = 36
        versionCode = 25
        versionName = "1.7.0"
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // Signed with the debug key so the APK can be sideloaded right away.
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    androidResources {
        // Music is streamed straight from the APK (AssetFileDescriptor), so keep it uncompressed.
        noCompress += listOf("mp3")
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
