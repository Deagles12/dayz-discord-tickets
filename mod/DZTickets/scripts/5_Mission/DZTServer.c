class DZTServer
{
    ref DZTServerConfig config;
    ref DZTStorage storage = new DZTStorage;
    ref map<string, int> lastRequest = new map<string, int>;
    ref DZTDiscord discord;
    bool enabled;

    void DZTServer()
    {
        MakeDirectory(DZTStorage.ROOT);
        MakeDirectory(DZTStorage.DIRECTORY);
        string path = DZTStorage.ROOT + "/config.json";
        string error;
        config = new DZTServerConfig;
        if (!FileExist(path) && !JsonFileLoader<DZTServerConfig>.SaveFile(path, config, error))
        {
            Print("[DZTickets] Cannot write server config.");
            return;
        }
        if (!JsonFileLoader<DZTServerConfig>.LoadFile(path, config, error) || !config || !config.categories || !config.admin_steam_ids)
        {
            Print("[DZTickets] Invalid server config.");
            return;
        }
        if (!storage.Load()) return;
        discord = new DZTDiscord(config);
        enabled = true;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Poll, 15000, true);
        Print("[DZTickets] Local ticket storage ready. Tickets: " + storage.ids.Count().ToString());
    }

    void Stop()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(Poll);
        if (discord) discord.Stop();
    }

    static bool IsSteamID(string value)
    {
        if (value.Length() != 17) return false;
        for (int i = 0; i < 17; i++)
            if ("0123456789".IndexOf(value.Substring(i, 1)) < 0) return false;
        return true;
    }

    void Receive(PlayerIdentity sender, string json)
    {
        DZTRequest request;
        string error;
        if (!sender || !JsonFileLoader<DZTRequest>.LoadData(json, request, error) || !request) return;
        string steam = sender.GetPlainId();
        if (!IsSteamID(steam)) return;
        DZTResponse response = new DZTResponse;
        response.op = request.op;
        response.request_id = request.request_id;
        if (!enabled)
            response.error = "Сховище підтримки недоступне.";
        else if (request.op == "ack")
        {
            Ack(steam, request.event_id);
            return;
        }
        else
        {
            int previous;
            int tick = GetGame().GetTime();
            bool testCreate = request.op == "create" && request.category == "ТЕСТ" && config.admin_steam_ids.Find(steam) >= 0;
            if (!testCreate && lastRequest.Find(steam, previous) && tick - previous >= 0 && tick - previous < 500)
                response.error = "Зачекайте мить перед наступною дією.";
            else
            {
                lastRequest.Set(steam, tick);
                string position;
                if (request.op == "create")
                {
                    array<Man> players = new array<Man>;
                    GetGame().GetPlayers(players);
                    foreach (Man player : players)
                    {
                        PlayerIdentity identity = player.GetIdentity();
                        if (identity && identity.GetPlainId() == steam)
                        {
                            position = player.GetPosition().ToString();
                            break;
                        }
                    }
                }
                response.error = Execute(steam, sender.GetName(), position, request, response);
            }
        }
        response.ok = response.error == "";
        if (JsonFileLoader<DZTResponse>.MakeData(response, json, error, false))
            DZTBus.SendPacket(DZT_RPC_RESPONSE, json, sender);
    }

    string Execute(string steam, string playerName, string position, DZTRequest request, DZTResponse response)
    {
        response.admin = config.admin_steam_ids.Find(steam) >= 0;
        foreach (string category : config.categories)
            if (category != "" && category != "ТЕСТ" && category.LengthUtf8() <= 60 && response.categories.Find(category) < 0 && response.categories.Count() < 32)
                response.categories.Insert(category);
        if (response.admin) response.categories.Insert("ТЕСТ");

        if (request.ticket_id < 0 || request.before < 0) return "Некоректний номер";
        if (request.op == "list" || request.op == "history")
        {
            if (request.op == "history" && !response.admin) return "Потрібні права адміністратора";
            if (request.op == "history" && !IsSteamID(request.owner)) return "Некоректний SteamID";
            List(steam, request, response);
            return "";
        }
        if (request.op != "create" && request.op != "detail" && request.op != "reply" && request.op != "claim" && request.op != "close")
            return "Невідома операція";

        DZTRecord record;
        if (request.op != "create")
        {
            if (!storage.records.Find(request.ticket_id, record) || (!response.admin && record.ticket.steam != steam))
                return "Тікет недоступний";
            if (request.op == "detail")
            {
                Detail(record, request.before, response);
                return "";
            }
        }

        if ((request.op == "claim" || request.op == "close") && !response.admin) return "Потрібні права адміністратора";
        if (request.op == "create" && response.categories.Find(request.category) < 0) return "Невідома категорія";
        if (request.op == "create" && !request.confirmed) return "Підтвердьте звернення галочкою перед надсиланням";
        request.body.TrimInPlace();
        if ((request.op == "create" || request.op == "reply") && (request.body == "" || request.body.LengthUtf8() > 2000))
            return "Потрібно від 1 до 2000 символів";
        if (request.request_id == "" || request.request_id.Length() > 100) return "Некоректний request_id";

        // Receipts live in the same saved revision as the mutation.
        foreach (int id : storage.ids)
        {
            DZTRecord candidate = storage.records.Get(id);
            foreach (DZTStoredEvent receipt : candidate.events)
            {
                if (receipt.actor != steam || receipt.request_id != request.request_id) continue;
                if (receipt.request_op != request.op) return "request_id вже використано для іншої операції";
                if (request.op == "create")
                {
                    if (candidate.ticket.category != request.category || receipt.body != request.body)
                        return "request_id вже використано для іншої операції";
                }
                else if (request.ticket_id != id || (request.op == "reply" && receipt.body != request.body))
                    return "request_id вже використано для іншої операції";
                response.ticket_id = id;
                return "";
            }
        }

        string stamp;
        int now = DZTStorage.Now(stamp);
        if (request.op == "create")
        {
            if (request.category != "ТЕСТ")
            {
                int active;
                foreach (int ticketId : storage.ids)
                {
                    DZTRecord own = storage.records.Get(ticketId);
                    if (own.ticket.steam != steam || own.ticket.category == "ТЕСТ") continue;
                    if (own.ticket.status != "closed") active++;
                    if (now - own.time_seconds < config.create_cooldown_seconds && config.create_cooldown_seconds > 0)
                        return "Зачекайте перед створенням наступного тікета";
                }
                if (active >= Math.Max(1, config.max_active)) return "Досягнуто ліміт активних звернень";
            }
            record = new DZTRecord;
            record.ticket = new DZTTicket;
            record.ticket.id = storage.lastTicket + 1;
            record.ticket.steam = steam;
            record.ticket.name = playerName;
            record.ticket.position = position;
            record.ticket.category = request.category;
            record.ticket.status = "open";
            record.ticket.created = stamp;
            record.time_seconds = now;
        }
        else if (record.ticket.status == "closed") return "Тікет уже закрито";

        if (request.op == "reply" && config.reply_cooldown_seconds > 0)
        {
            foreach (int replyId : storage.ids)
            {
                DZTRecord threadRecord = storage.records.Get(replyId);
                for (int i = threadRecord.events.Count() - 1; i >= 0; i--)
                {
                    DZTStoredEvent previousEvent = threadRecord.events[i];
                    if (now - previousEvent.time_seconds >= config.reply_cooldown_seconds) break;
                    if (previousEvent.actor == steam && (previousEvent.kind == "reply" || previousEvent.kind == "create"))
                        return "Повідомлення надсилаються надто часто";
                }
            }
        }
        if (request.op == "claim" && record.ticket.assignee != "" && record.ticket.assignee != steam)
            return "Тікет уже взяв інший адміністратор";

        if (storage.lastEvent >= 2147483646 || record.ticket.id <= 0) return "Вичерпано номери звернень.";
        string oldStatus = record.ticket.status;
        string oldAssignee = record.ticket.assignee;
        DZTStoredEvent entry = new DZTStoredEvent;
        entry.id = storage.lastEvent + 1;
        entry.ticket = record.ticket.id;
        entry.actor = steam;
        entry.actor_name = playerName;
        entry.steam = record.ticket.steam;
        entry.created = stamp;
        entry.time_seconds = now;
        entry.kind = request.op;
        entry.body = request.body;
        entry.request_id = request.request_id;
        entry.request_op = request.op;
        entry.player_ack = request.op == "create" || (request.op == "reply" && steam == record.ticket.steam);
        if (request.op == "claim" || request.op == "close")
        {
            entry.kind = "status";
            if (request.op == "claim")
            {
                record.ticket.status = "in_progress";
                record.ticket.assignee = steam;
                if (oldStatus == "in_progress")
                {
                    entry.kind = "receipt";
                    entry.player_ack = true;
                }
            }
            else record.ticket.status = "closed";
            entry.body = record.ticket.status;
        }
        record.events.Insert(entry);
        if (!storage.Commit(record))
        {
            record.events.Remove(record.events.Count() - 1);
            record.ticket.status = oldStatus;
            record.ticket.assignee = oldAssignee;
            return "Не вдалося зберегти звернення. Спробуйте ще раз.";
        }
        storage.lastEvent = entry.id;
        if (request.op == "create")
        {
            storage.lastTicket = record.ticket.id;
            storage.records.Set(record.ticket.id, record);
            storage.ids.Insert(record.ticket.id);
        }
        response.ticket_id = record.ticket.id;
        if (entry.kind != "receipt") discord.Notify(entry, record.ticket, playerName);
        return "";
    }

    void List(string steam, DZTRequest request, DZTResponse response)
    {
        string owner = steam;
        if (request.op == "history") owner = request.owner;
        bool allOpen = response.admin && request.all_open && request.op == "list";
        for (int i = storage.ids.Count() - 1; i >= 0; i--)
        {
            int id = storage.ids[i];
            if (request.before > 0 && id >= request.before) continue;
            DZTRecord record = storage.records.Get(id);
            if (allOpen && record.ticket.status == "closed") continue;
            if (!allOpen && record.ticket.steam != owner) continue;
            if (response.tickets.Count() == 30)
            {
                response.has_more = true;
                break;
            }
            response.tickets.Insert(record.ticket);
            response.next_before = id;
        }
    }

    void Detail(DZTRecord record, int before, DZTResponse response)
    {
        response.ticket_id = record.ticket.id;
        response.tickets.Insert(record.ticket);
        for (int i = record.events.Count() - 1; i >= 0; i--)
        {
            DZTStoredEvent entry = record.events[i];
            if (entry.kind == "receipt" || (before > 0 && entry.id >= before)) continue;
            if (response.messages.Count() == 5)
            {
                response.has_more = true;
                break;
            }
            DZTMessage message = entry;
            response.messages.InsertAt(message, 0);
            response.next_before = entry.id;
        }
    }

    void Ack(string steam, int eventId)
    {
        if (eventId <= 0) return;
        foreach (int id : storage.ids)
        {
            DZTRecord record = storage.records.Get(id);
            if (record.ticket.steam != steam) continue;
            foreach (DZTStoredEvent entry : record.events)
            {
                if (entry.id != eventId || entry.player_ack) continue;
                entry.player_ack = true;
                if (!storage.Commit(record)) entry.player_ack = false;
                return;
            }
        }
    }

    void Poll()
    {
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        ref map<string, DZTStoredEvent> pending = new map<string, DZTStoredEvent>;
        foreach (int id : storage.ids)
        {
            DZTRecord record = storage.records.Get(id);
            foreach (DZTStoredEvent entry : record.events)
            {
                if (entry.player_ack) continue;
                DZTStoredEvent oldest;
                if (!pending.Find(record.ticket.steam, oldest) || entry.id < oldest.id)
                    pending.Set(record.ticket.steam, entry);
                break;
            }
        }
        foreach (Man player : players)
        {
            PlayerIdentity identity = player.GetIdentity();
            DZTStoredEvent message;
            if (!identity || !pending.Find(identity.GetPlainId(), message)) continue;
            string json, error;
            if (JsonFileLoader<DZTMessage>.MakeData(message, json, error, false))
                DZTBus.SendPacket(DZT_RPC_NOTIFY, json, identity);
        }
    }
}

modded class MissionServer
{
    ref DZTServer dztServer;

    override void OnInit()
    {
        super.OnInit();
        dztServer = new DZTServer;
        DZTBus.Server.Insert(dztServer.Receive);
    }

    override void OnMissionFinish()
    {
        if (dztServer)
        {
            DZTBus.Server.Remove(dztServer.Receive);
            dztServer.Stop();
        }
        super.OnMissionFinish();
    }
}


