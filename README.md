# DayZ Discord Tickets

DayZ Discord Tickets adds an in-game support panel for players and server staff. Tickets are stored on the DayZ server. An optional outgoing Discord webhook relays ticket activity to a staff channel.

[Steam Workshop](https://steamcommunity.com/sharedfiles/filedetails/?id=3832912523)

## Features

- Players can create categorized tickets, view their own history, and reply to open tickets. The panel opens with **F7** by default.
- Staff can view open tickets, claim them, reply, close them, and look up a player's ticket history by SteamID64. Staff can also see the player's position at the time the ticket was created.
- The panel supports English, Ukrainian, and German. Conversations and ticket lists are paginated, and the current conversation page can be copied.
- Optional Discord notifications cover ticket creation, replies, and status changes. Built-in labels, statuses, and standard categories are in English. Player text and custom category labels retain their original wording.

## Installation

1. Subscribe to the Workshop item and install the same version on the server and all connecting clients.
2. Load the mod with DayZ's `-mod` option on both sides. Use the installed mod folder name in the argument, for example `-mod=@DayZTicketSystem`.
3. If the server verifies signatures, copy the Workshop package's public `.bikey` from its `Keys` folder into the server's `keys` folder.
4. Start the server once to generate `$profile:DayZDiscordTickets/config.json`. Stop the server, edit the file, then restart it.
5. Press **F7** in game. Select a category, enter a description, tick the confirmation box, and submit. Staff commands appear for SteamID64s listed in the mod's server configuration.

## Server configuration

The [example configuration](config/config.example.json) uses English category labels and leaves Discord disabled. Copy settings into the server's generated configuration as needed.

| Setting | Default | Purpose |
| --- | --- | --- |
| `admin_steam_ids` | Empty list | SteamID64 strings for ticket administrators. Permissions are configured independently of other admin mods. |
| `categories` | Complaint, Bug, Suspected cheating, Other | Labels for new tickets. The generated configuration stores the built-in labels in Ukrainian and the UI translates them. |
| `max_active` | 3 | Maximum active tickets per player. |
| `create_cooldown_seconds` | 60 | Minimum time between new tickets. |
| `reply_cooldown_seconds` | 3 | Minimum time between replies. |
| `discord_enabled` | `false` | Enables outgoing webhook notifications. |
| `discord_webhook_url` | Empty | Discord webhook URL. Set it only in the private server configuration. |

Ticket descriptions and replies are limited to 2,000 characters. Discord messages may contain player names and ticket text, so use a staff-only channel. Staff manage tickets in DayZ. The webhook has no Discord reply integration or persistent delivery queue.

Back up `$profile:DayZDiscordTickets/tickets` before server maintenance. The server keeps two JSON revision slots per ticket.

## Dependencies and compatibility

- Runtime dependencies are DayZ's `DZ_Data` and `DZ_Scripts` addons. CF and VPPAdminTools are optional and were present in the tested setup.
- Version 1.0.3 was checked on Windows with DayZ 1.30.164289.70 and CF/VPPAdminTools. Server checks covered webhook payload creation. Other game builds and mod combinations have not been verified here.
- DayZ Tools is required to package and sign a source build. Installation from Workshop uses the published package.

## Package from source

1. Extract `DZTickets.pbo` from the installed Workshop mod with a DayZ PBO tool. Copy `gui/fonts/sdf_MetronBook24.fnt` and `gui/fonts/sdf_MetronBook24.edds` from that PBO into `mod/DZTickets/gui/fonts/`. These game-derived assets stay local.
2. In DayZ Tools Addon Builder, pack `mod/DZTickets` with the internal PBO prefix `DZTickets`. Use pack-only mode and include `.c`, `.layout`, `.xml`, `.json`, `.fnt`, and `.edds` resources.
3. Place `DZTickets.pbo` in `@DayZTicketSystem/Addons/` and copy `mod/mod.cpp` to `@DayZTicketSystem/mod.cpp`. Sign your build with your own key for a server that verifies signatures, and install only the corresponding public key on the server.

## License

The source code is available under the [MIT License](LICENSE). Game assets used by local builds are subject to their own terms.
