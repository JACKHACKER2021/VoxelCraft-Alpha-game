plugins {
    id("com.android.application")
}

android {
    namespace = "cz.voxelcraft"
    compileSdk = 36
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "cz.voxelsoftworks.voxelcraft"  // 2.4.0: new id and a lasting signing key (the old key was lost)
        minSdk = 30
        targetSdk = 36
        versionCode = 33
        versionName = "2.4.0"
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
            ndk { abiFilters += listOf("arm64-v8a", "x86_64") }
        }
        create("preview") {
            dimension = "edition"
            applicationIdSuffix = ".preview"
            versionName = "2.5.0"  // the preview runs ahead of the normal edition
            versionCode = 36
            versionNameSuffix = "-preview"
            resValue("string", "app_name", "VoxelCraft Preview")
            ndk { abiFilters += listOf("arm64-v8a", "x86_64") }
            externalNativeBuild { cmake { arguments += listOf("-DVC_PREVIEW=ON") } }
        }
        // VoxelCraft Classic 0.0.3: the old game as its own small app (at most 20 MB): the old world,
        // the old look and the textures of the first HTML version; no music files, no Rozhlas voice.
        create("classic") {
            dimension = "edition"
            applicationIdSuffix = ".classic"
            versionCode = 3
            versionName = "0.0.3"
            resValue("string", "app_name", "VoxelCraft Classic")
            ndk { abiFilters += listOf("arm64-v8a") }  // phones only: keeps the APK small
            externalNativeBuild { cmake { arguments += listOf("-DVC_CLASSIC=ON") } }
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

    // Classic leaves out the big assets (its sounds are synthesized, as they were back then).
    sourceSets {
        getByName("classic") { assets.srcDirs("src/classic/assets") }
    }
    applicationVariants.all {
        if (flavorName == "classic") {
            mergeAssetsProvider.configure {
                doLast {
                    val out = outputDir.get().asFile
                    listOf("espeak-ng-data", "trailers").forEach { project.delete(out.resolve(it)) }
                    // Only its own menu song (src/classic/assets/music/menu_theme.mp3) stays.
                    out.resolve("music").listFiles()?.filter { it.name != "menu_theme.mp3" }?.forEach { project.delete(it) }
                }
            }
        }
    }
    packaging { jniLibs { keepDebugSymbols.clear() } }

    androidResources {
        // Music is streamed straight from the APK (AssetFileDescriptor), so keep it uncompressed.
        noCompress += listOf("mp3")
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
