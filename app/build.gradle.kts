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
        versionCode = 29
        versionName = "2.0.0"
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

    // 2.1.0: two editions from one code base. "normal" is the stable game (built by GitHub Actions
    // into release/), "preview" is the test build with the next version's features (built in chat),
    // installed beside it as its own app with its own worlds.
    flavorDimensions += "edition"
    productFlavors {
        create("normal") {
            dimension = "edition"
            resValue("string", "app_name", "VoxelCraft")
        }
        create("preview") {
            dimension = "edition"
            applicationIdSuffix = ".preview"
            versionNameSuffix = "-preview"
            resValue("string", "app_name", "VoxelCraft Preview")
            externalNativeBuild { cmake { arguments += listOf("-DVC_PREVIEW=ON") } }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // Signed with the debug key so the APK can be sideloaded right away.
            signingConfig = signingConfigs.getByName("debug")
            // GitHub Actions signs the normal edition with the project key (secrets), see
            // .github/workflows/release.yml: then every release installs over the one before.
            System.getenv("VC_KEYSTORE")?.let { path ->
                signingConfig = signingConfigs.create("project") {
                    storeFile = file(path)
                    storePassword = System.getenv("VC_KEYSTORE_PASSWORD")
                    keyAlias = System.getenv("VC_KEY_ALIAS")
                    keyPassword = System.getenv("VC_KEY_PASSWORD")
                }
            }
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
