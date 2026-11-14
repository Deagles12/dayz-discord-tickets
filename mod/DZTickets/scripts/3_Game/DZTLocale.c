class DZTLanguageSettings { int language = 0; }
class DZTLocale
{
    static const int UK = 0;
    static const int EN = 1;
    static const int DE = 2;
    static ref DZTLanguageSettings settings;
    static int Language()
    {
        if (!settings)
        {
            settings = new DZTLanguageSettings;
            string error;
            if (FileExist("$profile:AdminTicketSystemLanguage.json"))
                JsonFileLoader<DZTLanguageSettings>.LoadFile("$profile:AdminTicketSystemLanguage.json", settings, error);
            if (!settings) settings = new DZTLanguageSettings;
            if (settings.language < UK || settings.language > DE) settings.language = UK;
        }
        return settings.language;
    }
    static void Select(int language)
    {
        Language();
        settings.language = Math.Clamp(language, UK, DE);
        string error;
        JsonFileLoader<DZTLanguageSettings>.SaveFile("$profile:AdminTicketSystemLanguage.json", settings, error);
    }
    static string Name()
    {
        if (Language() == 1) return "English";
        if (Language() == 2) return "Deutsch";
        return "Українська";
    }
    static string Readable(string value)
    {
        value.Replace("І", "I");
        value.Replace("і", "i");
        return value;
    }

    static string T(string value)
    {
        return Readable(Translate(value));
    }

    static string Translate(string value)
    {
        int lang = Language();
        if (lang == 0) return value;
        if (value.Contains(": потрібно від 1 до"))
        {
            if (lang == 1) return "Check the required fields and text length (1-2000 characters).";
            return "Prüfe die Pflichtfelder und Textlänge (1-2000 Zeichen).";
        }
        if (lang == DE) return German(value);
        return English(value);
    }

    static string English(string value)
    {
        if (value == "Опис") return "Description";
        if (value == "Немає звернень") return "No tickets";
        if (value == "Підтвердити надсилання") return "Confirm submission";
        if (value == "Час гри: ") return "Game time: ";
        if (value == "Мої") return "Mine";
        if (value == "Усі відкриті") return "All open";
        if (value == "У списку: ") return "Listed: ";
        if (value == "Виділення тексту") return "Text selection";
        if (value == "Скопійовано") return "Copied";
        if (value == "Мої звернення") return "My tickets";
        if (value == "Категорія") return "Category";
        if (value == "Що сталося?") return "What happened?";
        if (value == "Ваша відповідь") return "Your reply";
        if (value == "Додатково") return "More";
        if (value == "Дії") return "Actions";
        if (value == "Скасувати") return "Cancel";
        if (value == "Так, закрити") return "Yes, close";
        if (value == "Закрити це звернення?") return "Close this ticket?";
        if (value == "Після закриття відповіді будуть недоступні. Історія листування збережеться.") return "Replies will be disabled. The conversation history will be kept.";

        if (value == "Дані звернення") return "Ticket details";
        if (value == "Сховище підтримки недоступне.") return "Ticket storage unavailable.";
        if (value == "Не вдалося зберегти звернення. Спробуйте ще раз.") return "Could not save. Please try again.";
        if (value == "Некоректний request_id") return "Invalid request ID";
        if (value == "Вичерпано номери звернень.") return "Ticket IDs exhausted.";
        if (value == "Потрібно від 1 до 2000 символів") return "Enter 1–2000 characters";
        if (value == "ПІДТРИМКА") return "SUPPORT";
        if (value == "Залишайтеся в грі. Ми на зв'язку.") return "Stay in game. We are here to help.";
        if (value == "+ Нове звернення") return "+ New ticket";
        if (value == "МОЇ ЗВЕРНЕННЯ") return "MY TICKETS";
        if (value == "Поки що порожньо. Створіть перше звернення.") return "No tickets yet. Create your first ticket.";
        if (value == "Копіювати") return "Copy";
        if (value == "Виділити") return "Select";
        if (value == "Перегляд") return "View";
        if (value == "Раніші") return "Older";
        if (value == "Виділи потрібний текст і натисни Ctrl+C.") return "Select the text and press Ctrl+C.";
        if (value == "Копіювати текст") return "Copy text";
        if (value == "Видимі повідомлення скопійовано.") return "Visible messages copied.";
        if (value == "Відкрити") return "Open";
        if (value == "Оновити") return "Refresh";
        if (value == "Раніші звернення") return "Older tickets";
        if (value == "Усі відкриті / Адмін") return "All open / Admin";
        if (value == "Історія") return "History";
        if (value == "Нове звернення") return "New ticket";
        if (value == "Коротко опишіть проблему - адміністрація допоможе.") return "Describe the issue briefly. We will help.";
        if (value == "КАТЕГОРІЯ") return "CATEGORY";
        if (value == "ЩО СТАЛОСЯ?") return "WHAT HAPPENED?";
        if (value == "Я перевірив опис і підтверджую надсилання.") return "I checked the details and confirm submission.";
        if (value == "Надіслати звернення") return "Submit ticket";
        if (value == "Дані гравця додадуться автоматично.") return "Player details are added automatically.";
        if (value == "Листування") return "Conversation";
        if (value == "Раніші повідомлення") return "Older messages";
        if (value == "ВАША ВІДПОВІДЬ") return "YOUR REPLY";
        if (value == "Взяти в роботу") return "Claim";
        if (value == "Закрити тікет") return "Close ticket";
        if (value == "Відповісти") return "Reply";
        if (value == "Підключення...") return "Connecting...";
        if (value == "Завантаження...") return "Loading...";
        if (value == "Нове") return "Open";
        if (value == "У роботі") return "In progress";
        if (value == "Закрите") return "Closed";
        if (value == "Підтримка") return "Support";
        if (value == "Гравець") return "Player";
        if (value == "Статус: ") return "Status: ";
        if (value == "Звернення #") return "Ticket #";
        if (value == "Тікет #") return "Ticket #";
        if (value == " збережено.") return " saved.";
        if (value == "Таймаут. Оновіть список перед повторним надсиланням.") return "Timed out. Refresh before submitting again.";
        if (value == "Ми тут, щоб допомогти. Опишіть, що сталося.") return "We are here to help. Tell us what happened.";
        if (value == "Звернення закрито. Листування залишається в історії.") return "Ticket closed. Conversation saved in history.";
        if (value == "Відповіді адміністрації з'являться тут автоматично.") return "Staff replies appear here automatically.";
        if (value == "Нове звернення. Оберіть категорію й опишіть ситуацію.") return "Choose a category and describe the issue.";
        if (value == "Поставте галочку підтвердження перед надсиланням.") return "Tick the confirmation box before submitting.";
        if (value == "Спочатку відкрийте тікет.") return "Open a ticket first.";
        if (value == "Нова відповідь або статус. F7 - відкрити підтримку.") return "New reply or status. F7 opens support.";
        if (value == "ТЕСТ") return "TEST";
        if (value == "Скарга") return "Complaint";
        if (value == "Баг") return "Bug";
        if (value == "Підозра на чітера") return "Suspected cheating";
        if (value == "Інше") return "Other";
        if (value == "Некоректний номер") return "Invalid number";
        if (value == "Тікет недоступний") return "Ticket unavailable";
        if (value == "request_id вже використано для іншої операції") return "Request ID already used";
        if (value == "Підтвердьте звернення галочкою перед надсиланням") return "Confirm before submitting";
        if (value == "Некоректний SteamID") return "Invalid SteamID";
        if (value == "Невідома категорія") return "Unknown category";
        if (value == "Досягнуто ліміт активних звернень") return "Active ticket limit reached";
        if (value == "Зачекайте перед створенням наступного тікета") return "Wait before creating another ticket";
        if (value == "Тікет уже закрито") return "Ticket already closed";
        if (value == "Повідомлення надсилаються надто часто") return "Wait before sending again";
        if (value == "Потрібні права адміністратора") return "Admin access required";
        if (value == "Тікет уже взяв інший адміністратор") return "Another admin has claimed this ticket";
        if (value == "Невідома операція") return "Unknown operation";
        if (value == "Некоректний список гравців") return "Invalid player list";
        if (value == "Сервіс підтримки не налаштовано.") return "Support service is not configured.";
        if (value == "Зачекайте мить перед наступною дією.") return "Wait a moment before the next action.";
        if (value == "Черга заповнена. Спробуйте пізніше.") return "Queue full. Try again later.";
        if (value == "Немає зв’язку. Перевірте історію перед повторним надсиланням.") return "Connection lost. Check history before resubmitting.";
        return value;
    }

    static string German(string value)
    {
        if (value == "Опис") return "Beschreibung";
        if (value == "Немає звернень") return "Keine Tickets";
        if (value == "Підтвердити надсилання") return "Senden bestaetigen";
        if (value == "Час гри: ") return "Spielzeit: ";
        if (value == "Мої") return "Eigene";
        if (value == "Усі відкриті") return "Alle offenen";
        if (value == "У списку: ") return "In Liste: ";
        if (value == "Виділення тексту") return "Textauswahl";
        if (value == "Скопійовано") return "Kopiert";
        if (value == "Мої звернення") return "Meine Tickets";
        if (value == "Категорія") return "Kategorie";
        if (value == "Що сталося?") return "Was ist passiert?";
        if (value == "Ваша відповідь") return "Deine Antwort";
        if (value == "Додатково") return "Weitere";
        if (value == "Дії") return "Aktionen";
        if (value == "Скасувати") return "Abbrechen";
        if (value == "Так, закрити") return "Ja, schließen";
        if (value == "Закрити це звернення?") return "Dieses Ticket schließen?";
        if (value == "Після закриття відповіді будуть недоступні. Історія листування збережеться.") return "Antworten sind danach nicht mehr möglich. Der Verlauf bleibt erhalten.";

        if (value == "Дані звернення") return "Ticketdaten";
        if (value == "Сховище підтримки недоступне.") return "Ticketspeicher nicht verfügbar.";
        if (value == "Не вдалося зберегти звернення. Спробуйте ще раз.") return "Speichern fehlgeschlagen. Bitte erneut versuchen.";
        if (value == "Некоректний request_id") return "Ungültige Anfrage-ID";
        if (value == "Вичерпано номери звернень.") return "Keine Ticketnummern mehr verfügbar.";
        if (value == "Потрібно від 1 до 2000 символів") return "1–2000 Zeichen eingeben";
        if (value == "ПІДТРИМКА") return "SUPPORT";
        if (value == "Залишайтеся в грі. Ми на зв'язку.") return "Bleib im Spiel. Wir helfen dir.";
        if (value == "+ Нове звернення") return "+ Neues Ticket";
        if (value == "МОЇ ЗВЕРНЕННЯ") return "MEINE TICKETS";
        if (value == "Поки що порожньо. Створіть перше звернення.") return "Noch keine Tickets. Erstelle dein erstes Ticket.";
        if (value == "Копіювати") return "Kopieren";
        if (value == "Виділити") return "Auswählen";
        if (value == "Перегляд") return "Ansicht";
        if (value == "Раніші") return "Ältere";
        if (value == "Виділи потрібний текст і натисни Ctrl+C.") return "Text auswählen und Strg+C drücken.";
        if (value == "Копіювати текст") return "Text kopieren";
        if (value == "Видимі повідомлення скопійовано.") return "Sichtbare Nachrichten kopiert.";
        if (value == "Відкрити") return "Öffnen";
        if (value == "Оновити") return "Aktualisieren";
        if (value == "Раніші звернення") return "Ältere Tickets";
        if (value == "Усі відкриті / Адмін") return "Alle offenen / Admin";
        if (value == "Історія") return "Verlauf";
        if (value == "Нове звернення") return "Neues Ticket";
        if (value == "Коротко опишіть проблему - адміністрація допоможе.") return "Beschreibe kurz das Problem. Wir helfen dir.";
        if (value == "КАТЕГОРІЯ") return "KATEGORIE";
        if (value == "ЩО СТАЛОСЯ?") return "WAS IST PASSIERT?";
        if (value == "Я перевірив опис і підтверджую надсилання.") return "Ich habe die Angaben geprüft und bestätige das Senden.";
        if (value == "Надіслати звернення") return "Ticket senden";
        if (value == "Дані гравця додадуться автоматично.") return "Spielerdaten werden automatisch ergänzt.";
        if (value == "Листування") return "Nachrichten";
        if (value == "Раніші повідомлення") return "Ältere Nachrichten";
        if (value == "ВАША ВІДПОВІДЬ") return "DEINE ANTWORT";
        if (value == "Взяти в роботу") return "Übernehmen";
        if (value == "Закрити тікет") return "Ticket schließen";
        if (value == "Відповісти") return "Antworten";
        if (value == "Підключення...") return "Verbinden...";
        if (value == "Завантаження...") return "Laden...";
        if (value == "Нове") return "Offen";
        if (value == "У роботі") return "In Bearbeitung";
        if (value == "Закрите") return "Geschlossen";
        if (value == "Підтримка") return "Support";
        if (value == "Гравець") return "Spieler";
        if (value == "Статус: ") return "Status: ";
        if (value == "Звернення #") return "Ticket #";
        if (value == "Тікет #") return "Ticket #";
        if (value == " збережено.") return " gespeichert.";
        if (value == "Таймаут. Оновіть список перед повторним надсиланням.") return "Zeitüberschreitung. Vor erneutem Senden aktualisieren.";
        if (value == "Ми тут, щоб допомогти. Опишіть, що сталося.") return "Wir helfen dir. Beschreibe, was passiert ist.";
        if (value == "Звернення закрито. Листування залишається в історії.") return "Ticket geschlossen. Nachrichten bleiben im Verlauf.";
        if (value == "Відповіді адміністрації з'являться тут автоматично.") return "Antworten des Teams erscheinen hier automatisch.";
        if (value == "Нове звернення. Оберіть категорію й опишіть ситуацію.") return "Wähle eine Kategorie und beschreibe das Problem.";
        if (value == "Поставте галочку підтвердження перед надсиланням.") return "Bestätige das Kontrollkästchen vor dem Senden.";
        if (value == "Спочатку відкрийте тікет.") return "Öffne zuerst ein Ticket.";
        if (value == "Нова відповідь або статус. F7 - відкрити підтримку.") return "Neue Antwort oder Status. F7 öffnet den Support.";
        if (value == "ТЕСТ") return "TEST";
        if (value == "Скарга") return "Beschwerde";
        if (value == "Баг") return "Fehler";
        if (value == "Підозра на чітера") return "Cheat-Verdacht";
        if (value == "Інше") return "Sonstiges";
        if (value == "Некоректний номер") return "Ungültige Nummer";
        if (value == "Тікет недоступний") return "Ticket nicht verfügbar";
        if (value == "request_id вже використано для іншої операції") return "Anfrage-ID bereits verwendet";
        if (value == "Підтвердьте звернення галочкою перед надсиланням") return "Bestätige vor dem Senden";
        if (value == "Некоректний SteamID") return "Ungültige SteamID";
        if (value == "Невідома категорія") return "Unbekannte Kategorie";
        if (value == "Досягнуто ліміт активних звернень") return "Limit offener Tickets erreicht";
        if (value == "Зачекайте перед створенням наступного тікета") return "Warte vor dem nächsten Ticket";
        if (value == "Тікет уже закрито") return "Ticket bereits geschlossen";
        if (value == "Повідомлення надсилаються надто часто") return "Warte vor der nächsten Nachricht";
        if (value == "Потрібні права адміністратора") return "Adminrechte erforderlich";
        if (value == "Тікет уже взяв інший адміністратор") return "Ein anderer Admin bearbeitet dieses Ticket";
        if (value == "Невідома операція") return "Unbekannte Aktion";
        if (value == "Некоректний список гравців") return "Ungültige Spielerliste";
        if (value == "Сервіс підтримки не налаштовано.") return "Supportdienst nicht eingerichtet.";
        if (value == "Зачекайте мить перед наступною дією.") return "Warte kurz vor der nächsten Aktion.";
        if (value == "Черга заповнена. Спробуйте пізніше.") return "Warteschlange voll. Versuche es später erneut.";
        if (value == "Немає зв’язку. Перевірте історію перед повторним надсиланням.") return "Keine Verbindung. Prüfe den Verlauf vor erneutem Senden.";
        return value;
    }
}
