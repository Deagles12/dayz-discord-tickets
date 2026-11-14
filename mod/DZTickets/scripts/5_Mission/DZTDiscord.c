class DZTWebhookField
{
    string name;
    string value;

    void DZTWebhookField(string label, string text)
    {
        name = label;
        value = text;
    }
}

class DZTWebhookEmbed
{
    string title;
    string description;
    int color = 4099830;
    ref array<ref DZTWebhookField> fields = new array<ref DZTWebhookField>;
}

class DZTWebhookPayload
{
    ref array<ref DZTWebhookEmbed> embeds = new array<ref DZTWebhookEmbed>;
    ref DZTWebhookMentions allowed_mentions = new DZTWebhookMentions;
}

class DZTWebhookMentions
{
    ref array<string> parse = new array<string>;
}

class DZTWebhookCallback : RestCallback
{
    DZTDiscord owner;

    override void OnSuccess(string data, int dataSize) { if (owner) owner.Completed(true); }
    override void OnError(int errorCode) { if (owner) owner.Completed(false); }
    override void OnTimeout() { if (owner) owner.Completed(false); }
}

class DZTDiscord
{
    RestContext rest;
    string endpoint;
    ref DZTWebhookCallback callback;
    ref array<string> queue = new array<string>;
    bool busy;

    void DZTDiscord(DZTServerConfig config)
    {
        string prefix = "https://discord.com/api/webhooks/";
        string url = config.discord_webhook_url;
        if (!config.discord_enabled) return;
        if (url.IndexOf(prefix) != 0 || url.Length() <= prefix.Length())
        {
            Print("[DZTickets] Discord webhook URL is invalid.");
            return;
        }
        endpoint = url.Substring(prefix.Length(), url.Length() - prefix.Length());
        if (!GetRestApi()) CreateRestApi();
        rest = GetRestApi().GetRestContext(prefix);
        if (!rest) return;
        rest.SetHeader("application/json");
        callback = new DZTWebhookCallback;
        callback.owner = this;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Pump, 2000, true);
    }

    static bool MakePayload(DZTStoredEvent entry, DZTTicket ticket, string actorName, out string data)
    {
        DZTWebhookPayload payload = new DZTWebhookPayload;
        DZTWebhookEmbed card = new DZTWebhookEmbed;
        string action = "New ticket";
        if (entry.kind == "reply") action = "Reply";
        if (entry.kind == "status") action = "Status changed";
        card.title = string.Format("Ticket #%1 - %2", ticket.id, action);
        card.description = entry.body;
        if (entry.kind == "status")
        {
            if (entry.body == "open") card.description = "Open";
            if (entry.body == "in_progress") card.description = "In progress";
            if (entry.body == "closed") card.description = "Closed";
        }
        if (actorName == "") actorName = entry.actor;
        card.fields.Insert(new DZTWebhookField("Category", DZTLocale.English(ticket.category)));
        card.fields.Insert(new DZTWebhookField("Event author", actorName.SubstringUtf8(0, Math.Min(actorName.LengthUtf8(), 128))));
        card.fields.Insert(new DZTWebhookField("Time", entry.created));
        payload.embeds.Insert(card);
        string error;
        return JsonFileLoader<DZTWebhookPayload>.MakeData(payload, data, error, false);
    }

    void Notify(DZTStoredEvent entry, DZTTicket ticket, string actorName)
    {
        if (!rest) return;
        if (queue.Count() >= 32)
        {
            Print("[DZTickets] Discord notification queue is full.");
            return;
        }
        string data;
        if (MakePayload(entry, ticket, actorName, data))
            queue.Insert(data);
    }

    void Pump()
    {
        if (busy || queue.Count() == 0) return;
        busy = true;
        rest.POST(callback, endpoint, queue[0]);
        queue.Remove(0);
    }

    void Completed(bool success)
    {
        busy = false;
        if (!success) Print("[DZTickets] Discord notification delivery failed.");
    }

    void Stop()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(Pump);
        if (callback) callback.owner = null;
        if (rest) rest.reset();
    }
}


