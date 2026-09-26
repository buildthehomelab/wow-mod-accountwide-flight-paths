# Account-wide Flight Paths

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that shares discovered flight
paths between the characters on an account. Find a flight master on one character, and your other
characters can fly there the next time they log in.

## What gets shared

- Every flight path a character has discovered, in any continent.
- Only flight paths the character's own faction can use. Your Horde characters don't get
  Stormwind and your Alliance characters don't get Orgrimmar. Neutral flight paths (Booty Bay,
  Gadgetzan, Everlook and so on) go to both.
- The Ebon Hold and Shadow Vault flight paths only go to death knights.
- Not the old world from a new death knight, by default. Death knights are created knowing every
  flight path in Kalimdor and the Eastern Kingdoms, so they only share the Outland and Northrend
  paths they discover. Set `AccountWideFlightPaths.ShareDeathKnightStartNodes = 1` to share
  everything.

Flight paths are never taken away. A character that already knows one keeps it.

Knowing an Outland or Northrend flight path doesn't get a character to that continent, so
[mod-individual-progression](https://github.com/ZhengPeiRu21/mod-individual-progression) still
decides when an alt can go there.

Bots from mod-playerbots are skipped both ways: they don't add flight paths to their account and
don't get taught any.

## Install

Clone it into your AzerothCore `modules` folder **as `mod-accountwide-flight-paths`**, without the
repo's `wow-` prefix. AzerothCore finds the module's entry point from the folder name.

```bash
cd <azerothcore>/modules
git clone https://github.com/buildthehomelab/wow-mod-accountwide-flight-paths.git mod-accountwide-flight-paths
```

Rebuild the worldserver, then copy `conf/mod_accountwide_flight_paths.conf.dist` to your config
folder as `mod_accountwide_flight_paths.conf`. The table is created in the characters database on
the next start, as long as `Updates.EnableDatabases` still includes the characters database (it
does by default).

Existing characters already count: each one adds the flight paths it knows the first time it logs
in with the module installed. So log in once on the character that has explored the most, then log
in on your alts.

## Settings

| Setting | Default | What it does |
|---------|---------|--------------|
| `AccountWideFlightPaths.Enable` | `1` | Master switch. |
| `AccountWideFlightPaths.Announce` | `1` | On login, say in chat how many flight paths the character got from the others. |
| `AccountWideFlightPaths.ShareDeathKnightStartNodes` | `0` | Let death knights share the old-world flight paths they're created with. |

## How it works

- **Login:** the character's own flight paths are added to the account first. Then it's given
  every account flight path it doesn't know yet and is allowed to have.
- **Discovering a flight path:** it's added to the account right away.
- **Logout:** saves once more.
- **Deleting the account's last character:** the account's flight path list is cleared.

## License

MIT. See `LICENSE`.
