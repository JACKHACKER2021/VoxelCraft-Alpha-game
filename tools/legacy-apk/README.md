# Cesta A: zprovoznění vlastního originálního APK (např. 1.1.2)

> ⚠️ Jen pro **vlastní použití** s APK, které sis legálně pořídil.
> Upravené APK pořád obsahuje kód od Mojangu, takže ho nikomu neposílej a nikam nenahrávej.
> APK ani světy se do tohoto repozitáře nikdy necommitují.

## Co potřebuješ
- PC s `java`, `adb` a Android build-tools (`zipalign`, `apksigner`, `aapt2`).
  Nejjednodušší je nainstalovat Android Studio, nebo samotné „command line tools“.
- Telefon se zapnutým **USB laděním** (Nastavení → O telefonu → 7× klepnout na „Číslo sestavení“ → Možnosti pro vývojáře).

## 1. Zjisti, jestli to vůbec může běžet
```bash
./patch_legacy_apk.sh check minecraftpe-1.1.2.apk
```
Skript vypíše:
- pro které procesory APK obsahuje kód (`armeabi-v7a` = 32bit, `arm64-v8a` = 64bit),
- jestli tvůj telefon ještě umí 32bitové aplikace.

**Když APK nemá `arm64-v8a` a telefon nepodporuje 32bitové aplikace, nepomůže žádný patch.**
Pak zbývá jen VoxelCraft (cesta C).

## 2. Zkus instalaci bez úprav
Android 14+ blokuje aplikace se starým `targetSdk`, ale přes adb se ten blok dá obejít:
```bash
./patch_legacy_apk.sh install minecraftpe-1.1.2.apk
```

## 3. Když to nestačí: patch
```bash
./patch_legacy_apk.sh patch minecraftpe-1.1.2.apk            # nahradí původní aplikaci
./patch_legacy_apk.sh patch minecraftpe-1.1.2.apk --rename   # nainstaluje se vedle novější verze
```
Skript zvedne `targetSdk` na 28, APK znovu sestaví, zarovná a podepíše
tvým osobním klíčem (`personal.keystore`, vytvoří se automaticky).

### Zálohuj světy!
Před odinstalováním původní verze:
```bash
adb pull /sdcard/games/com.mojang/minecraftWorlds ./worlds-backup
```

## Známá omezení
- Přihlášení k Xbox Live, Realms a obchod už v 1.1.2 nefungují (servery jsou vypnuté).
- Pokud hra spadne hned po spuštění, pošli výstup `adb logcat -d | grep -iE "minecraft|DEBUG|crash"`
  a podíváme se na to.
