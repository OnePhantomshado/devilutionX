<p align="center">
<img width="554" src="https://user-images.githubusercontent.com/204594/113575181-c946a400-961d-11eb-8347-a8829fa3830c.png">
</p>

---

[![Discord Channel](https://img.shields.io/discord/518540764754608128?color=%237289DA&logo=discord&logoColor=%23FFFFFF)](https://discord.gg/devilutionx)
[![Downloads](https://img.shields.io/github/downloads/diasurgical/devilutionX/total.svg)](https://github.com/diasurgical/devilutionX/releases/latest)
[![Codecov](https://codecov.io/gh/diasurgical/devilutionX/branch/master/graph/badge.svg)](https://codecov.io/gh/diasurgical/devilutionX)

<p align="center">
<img width="838" src="https://github.com/user-attachments/assets/db6e94b1-a98b-413d-a109-1fb77dda34bd">
</p>

<sub>*(The health-bar and XP-bar are off by default but can be enabled in the [game settings](https://github.com/diasurgical/devilutionX/wiki/DevilutionX-diablo.ini-configuration-guide). Widescreen can also be disabled if preferred.)*</sub>

# What is DevilutionX

DevilutionX is a port of Diablo and Hellfire that strives to make it simple to run the game while providing engine improvements, bug fixes, and some optional quality of life features.

Check out the [manual](https://github.com/diasurgical/devilutionX/wiki) for available features and how to take advantage of them.

For a full list of changes, see our [changelog](docs/CHANGELOG.md).

# How to Install

Note: You'll need access to the data from the original game. If you don't have an original CD, you can [buy Diablo from GoG.com](https://www.gog.com/game/diablo) or Battle.net. Alternatively, you can use `spawn.mpq` from the [shareware](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/spawn.mpq) [[2]](http://ftp.blizzard.com/pub/demos/diablosw.exe) version, in place of `DIABDAT.MPQ`, to play the shareware portion of the game.

Download the latest [DevilutionX release](https://github.com/diasurgical/devilutionX/releases/latest) and extract the contents to a location of your choosing or [build from source](#building-from-source).

- Copy `DIABDAT.MPQ` from the CD or Diablo installation (or [extract it from the GoG installer](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer)) to the DevilutionX folder.
- To run the Diablo: Hellfire expansion, you will also need to copy `hellfire.mpq`, `hfmonk.mpq`, `hfmusic.mpq`, and `hfvoice.mpq`.

For more detailed instructions: [Installation Instructions](./docs/installing.md).

# Contributing

We are always looking for more people to help with [coding](docs/CONTRIBUTING.md), [documentation](https://github.com/diasurgical/devilutionX/wiki), [testing the latest builds](#test-builds), spreading the word, or simply just hanging out on our [Discord server](https://discord.gg/devilutionx).

# Mods

We hope to provide a good starting point for mods. In addition to the full Devilution source code, we also provide modding tools. Check out the list of known [mods based on DevilutionX](https://github.com/diasurgical/devilutionX/wiki/Mods).

DevilutionX - Custom Modding Project


Overview
This fork of DevilutionX introduces a flexible and user-friendly modding system to allow players and developers to customize gameplay, character classes, items, and more without requiring recompilation. The goal is to make DevilutionX more accessible to modders while maintaining the original charm of Diablo 1.

Goals
Simplified Modding:

Enable users to modify game configurations (e.g., player stats, items, and classes) using external files such as INI or XML.
Ensure changes can be applied without recompiling the game.
Expand Customization:

Allow the addition of new player classes, abilities, and items.
Include support for toggling custom classes on/off for easy experimentation.
Maintain Compatibility:

Retain the core DevilutionX experience while adding modding flexibility.
Ensure base game functionality is not disrupted.
User-Friendly:

Design the modding system with accessibility in mind, so even non-technical users can easily create and apply mods.
Modding System Features
External Modding Files:

Mods are stored in a dedicated mods/ folder outside the compiled program for easy access.
Supports both INI (for simple configurations) and XML (for complex data).
Dynamic Player Classes:

Edit base classes (Warrior, Rogue, Sorcerer) or add custom classes like Cleric or Druid.
Define attributes such as stats, starting items, mana, and abilities.
Toggle custom classes on/off via configuration files.
Easy Integration:

No need to recompile the game after modifying players.ini or players.xml.
Fallback Defaults.
Plan to tackle order player,items,spell

<ModConfig>
    <Enable>true</Enable>
    <Players>
        <!-- Warrior Class -->
        <Player class="Warrior" Enabled="true">
            <Stats>
                <Might>30</Might> <!-- Warrior base strength -->
                <Agility>20</Agility> <!-- Warrior base dexterity -->
                <Intellect>10</Intellect> <!-- Warrior base magic -->
                <Presence>25</Presence> <!-- Warrior base vitality -->
            </Stats>
            <Derived>
                <HealthBase>45</HealthBase> <!-- Derived from vitality -->
                <ManaBase>0</ManaBase> <!-- Warrior starts with no mana -->
            </Derived>
            <Items>
                <Weapon>ShortSword</Weapon> <!-- Warrior's starting weapon -->
                <Armor>QuiltedArmor</Armor> <!-- Warrior's starting armor -->
                <Shield>SmallShield</Shield> <!-- Warrior's starting shield -->
                <Potion>HealingPotion</Potion> <!-- Default starting potion -->
                <Spell>None</Spell> <!-- Warrior does not start with spells -->
            </Items>
            <GraphProfile>Warrior</GraphProfile>
        </Player>

        <!-- Rogue Class -->
        <Player class="Rogue" Enabled="true">
            <Stats>
                <Might>20</Might> <!-- Rogue base strength -->
                <Agility>30</Agility> <!-- Rogue base dexterity -->
                <Intellect>15</Intellect> <!-- Rogue base magic -->
                <Presence>20</Presence> <!-- Rogue base vitality -->
            </Stats>
            <Derived>
                <HealthBase>35</HealthBase> <!-- Derived from vitality -->
                <ManaBase>20</ManaBase> <!-- Rogue starts with some mana -->
            </Derived>
            <Items>
                <Weapon>ShortBow</Weapon> <!-- Rogue's starting weapon -->
                <Armor>LeatherArmor</Armor> <!-- Rogue's starting armor -->
                <Shield>None</Shield> <!-- Rogue does not use shields -->
                <Potion>HealingPotion</Potion> <!-- Default starting potion -->
                <Spell>None</Spell> <!-- Rogue does not start with spells -->
            </Items>
            <GraphProfile>Rogue</GraphProfile>
        </Player>

        <!-- Sorcerer Class -->
        <Player class="Sorcerer" Enabled="true">
            <Stats>
                <Might>15</Might> <!-- Sorcerer base strength -->
                <Agility>25</Agility> <!-- Sorcerer base dexterity -->
                <Intellect>35</Intellect> <!-- Sorcerer base magic -->
                <Presence>20</Presence> <!-- Sorcerer base vitality -->
            </Stats>
            <Derived>
                <HealthBase>30</HealthBase> <!-- Derived from vitality -->
                <ManaBase>35</ManaBase> <!-- Sorcerer starts with significant mana -->
            </Derived>
            <Items>
                <Weapon>Staff</Weapon> <!-- Sorcerer's starting weapon -->
                <Armor>Cloak</Armor> <!-- Sorcerer's starting armor -->
                <Shield>None</Shield> <!-- Sorcerer does not use shields -->
                <Potion>ManaPotion</Potion> <!-- Default starting potion -->
                <Spell>Firebolt</Spell> <!-- Sorcerer's starting spell -->
            </Items>
            <GraphProfile>Sorcerer</GraphProfile>
        </Player>

        <!-- Monk Class (Hellfire) -->
        <Player class="Monk" Enabled="false">
            <Stats>
                <Might>25</Might> <!-- Monk is strong but agile -->
                <Agility>25</Agility> <!-- Monk focuses on dexterity -->
                <Intellect>15</Intellect> <!-- Moderate magic use -->
                <Presence>20</Presence> <!-- Balanced vitality -->
            </Stats>
            <Derived>
                <HealthBase>40</HealthBase> <!-- Derived from vitality -->
                <ManaBase>20</ManaBase> <!-- Moderate starting mana -->
            </Derived>
            <Items>
                <Weapon>Staff</Weapon> <!-- Monk begins with a staff -->
                <Armor>QuiltedArmor</Armor> <!-- Light starting armor -->
                <Shield>None</Shield> <!-- Monks typically avoid shields -->
                <Potion>HealingPotion</Potion> <!-- Default starting potion -->
                <Spell>None</Spell> <!-- Monk does not start with spells -->
            </Items>
            <GraphProfile>Sorcerer</GraphProfile> <!-- Monk uses Sorcerer graphics -->
        </Player>

        <!-- Custom Class: Cleric -->
        <Player class="Cleric" Enabled="false">
            <Stats>
                <Might>20</Might> <!-- Strong but not Warrior-level -->
                <Agility>15</Agility> <!-- Agile enough to wear lighter armor -->
                <Intellect>20</Intellect> <!-- Good spellcasting ability -->
                <Presence>30</Presence> <!-- High presence for healing and resilience -->
            </Stats>
            <Derived>
                <HealthBase>40</HealthBase>
                <ManaBase>35</ManaBase>
            </Derived>
            <Items>
                <Weapon>Mace</Weapon> <!-- Cleric begins with a mace -->
                <Armor>Robe</Armor> <!-- Cleric begins with light armor -->
                <Shield>SmallShield</Shield> <!-- Cleric uses a shield -->
                <Potion>HealingPotion</Potion> <!-- Default starting potion -->
                <Spell>Heal</Spell> <!-- Cleric begins with a healing spell -->
            </Items>
            <GraphProfile>Warrior</GraphProfile> <!-- Uses Warrior's graphics as default -->
        </Player>
    </Players>
</ModConfig>



# Test Builds

If you want to help test the latest development version (make sure to back up your files, as these may contain bugs), you can fetch the test build artifact from one of the build servers:

*Note: You must be logged into GitHub to download the attachments!*

[![Linux x86_64](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86_64.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86_64.yml?query=branch%3Amaster)
[![Linux AArch64](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_aarch64.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_aarch64.yml?query=branch%3Amaster)
[![Linux x86](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86.yml?query=branch%3Amaster)
[![Linux x86_64 SDL1](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86_64_SDL1.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86_64_SDL1.yml?query=branch%3Amaster)
[![macOS x86_64](https://github.com/diasurgical/devilutionX/actions/workflows/macOS_x86_64.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/macOS_x86_64.yml?query=branch%3Amaster)
[![Windows MSVC x64](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MSVC_x64.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MSVC_x64.yml?query=branch%3Amaster)
[![Windows MinGW x64](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MinGW_x64.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MinGW_x64.yml?query=branch%3Amaster)
[![Windows MinGW x86](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MinGW_x86.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MinGW_x86.yml?query=branch%3Amaster)
[![Android](https://github.com/diasurgical/devilutionX/actions/workflows/Android.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Android.yml?query=branch%3Amaster)
[![iOS](https://github.com/diasurgical/devilutionX/actions/workflows/iOS.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/iOS.yml?query=branch%3Amaster)
[![PS4](https://github.com/diasurgical/devilutionX/actions/workflows/PS4.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/PS4.yml?query=branch%3Amaster)
[![Original Xbox](https://github.com/diasurgical/devilutionX/actions/workflows/xbox_nxdk.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/xbox_nxdk.yml?query=branch%3Amaster)
[![Xbox One/Series](https://github.com/diasurgical/devilutionX/actions/workflows/xbox_one.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/xbox_one.yml?query=branch%3Amaster)
[![Nintendo Switch](https://github.com/diasurgical/devilutionX/actions/workflows/switch.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/switch.yml)
[![Sony PlayStation Vita](https://github.com/diasurgical/devilutionX/actions/workflows/vita.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/vita.yml)
[![Nintendo 3DS](https://github.com/diasurgical/devilutionX/actions/workflows/3ds.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/3ds.yml)
[![Amiga M68K](https://github.com/diasurgical/devilutionX/actions/workflows/amiga-m68k.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/amiga-m68k.yml)

# Building from Source

Want to compile the program by yourself? Great! Simply follow the [build instructions](./docs/building.md).

# Credits

- The original Devilution project: [Devilution](https://github.com/diasurgical/devilution#credits)
- [Everyone](https://github.com/diasurgical/devilutionX/graphs/contributors) who worked on Devilution/DevilutionX
- [Nikolay Popov](https://www.instagram.com/nikolaypopovz/) for UI and graphics
- [WiAParker](https://wiaparker.pl/projekty/diablo-hellfire/) for the Polish voice pack
- And thanks to all who support the project, report bugs, and help spread the word ❤️

# Legal

DevilutionX is made publicly available and released under the Sustainable Use License (see [LICENSE](LICENSE.md)).

The source code in this repository is for non-commercial use only. If you use the source code, you may not charge others for access to it or any derivative work thereof.

Diablo® - Copyright © 1996 Blizzard Entertainment, Inc. All rights reserved. Diablo and Blizzard Entertainment are trademarks or registered trademarks of Blizzard Entertainment, Inc. in the U.S. and/or other countries.

DevilutionX and any of its maintainers are in no way associated with or endorsed by Blizzard Entertainment®.
