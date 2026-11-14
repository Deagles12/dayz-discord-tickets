class DZTMenu : UIScriptedMenu
{
    TextListboxWidget ticketList;
    int selectedRow = -1;
    XComboBoxWidget category;
    MultilineEditBoxWidget description;
    Widget messageStack;
    XComboBoxWidget languagePicker;
    float uiScale;
    bool menuReady;
    bool applyingLanguage;
    bool normalizingInput;
    bool sidebarToolsOpen;
    bool threadToolsOpen;
    bool closingTicket;
    bool scrollToLatest;
    bool viewingOlder;
    bool refreshPending;
    float refreshDelay;
    bool menuVisible;
    float headerElapsed;
    string lastHeaderText;
    string submittedBody;
    MultilineEditBoxWidget submittedInput;
    ref map<int, string> replyDrafts = new map<int, string>;
    Widget hoveredButton;
    Widget pressedButton;
    ref map<string, bool> controlStates = new map<string, bool>;
    int viewportWidth;
    int viewportHeight;
    MultilineTextWidget threadMeta;
    ref DZTResponse threadResponse;
    ScrollWidget conversationScroll;
    string clipboardText;
    MultilineEditBoxWidget threadSelection;
    bool selectingThread;
    TextWidget statusText;
    EditBoxWidget owner;
    CheckBoxWidget confirmation;
    ButtonWidget createButton;
    TextWidget counter;
    TextWidget threadTitle;
    bool composing = true;
    bool isAdmin;
    bool serverCategories;
    bool ticketClosed;
    ref array<ref DZTTicket> tickets = new array<ref DZTTicket>;
    ref array<string> categories = new array<string>;
    int selectedTicket;
    int listBefore;
    int messageBefore;
    bool listMore;
    bool messageMore;
    bool allOpen;
    string historyOwner;
    bool waiting;
    string expectedRequest;
    static ref DZTRequest retryMutation;
    float waitTime;

    override Widget Init()
    {
        Print("[DZTickets] Menu layout creation begin (font-format fix 2026-11-05).");
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("DZTickets/gui/tickets.layout");
        Print("[DZTickets] Menu layout creation returned.");
        if (!layoutRoot) return null;
        languagePicker = XComboBoxWidget.Cast(layoutRoot.FindAnyWidget("LanguagePicker"));
        ticketList = TextListboxWidget.Cast(layoutRoot.FindAnyWidget("TicketList"));
        category = XComboBoxWidget.Cast(layoutRoot.FindAnyWidget("Category"));
        description = MultilineEditBoxWidget.Cast(layoutRoot.FindAnyWidget("Description"));
        confirmation = CheckBoxWidget.Cast(layoutRoot.FindAnyWidget("Confirmation"));
        createButton = ButtonWidget.Cast(layoutRoot.FindAnyWidget("Create"));
        counter = TextWidget.Cast(layoutRoot.FindAnyWidget("Counter"));
        threadTitle = TextWidget.Cast(layoutRoot.FindAnyWidget("ThreadTitle"));
        messageStack = layoutRoot.FindAnyWidget("MessageStack");
        threadMeta = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("ThreadMeta"));
        threadSelection = MultilineEditBoxWidget.Cast(layoutRoot.FindAnyWidget("ThreadSelection"));
        conversationScroll = ScrollWidget.Cast(layoutRoot.FindAnyWidget("ConversationScroll"));
        statusText = TextWidget.Cast(layoutRoot.FindAnyWidget("Status"));
        owner = EditBoxWidget.Cast(layoutRoot.FindAnyWidget("Owner"));
        if (!languagePicker || !category || !counter || !threadTitle || !threadMeta || !ticketList || !layoutRoot.FindAnyWidget("ButtonMeasure") || !layoutRoot.FindAnyWidget("ReplyDescription") || !description || !confirmation || !createButton || !owner || !messageStack || !conversationScroll || !threadSelection || !statusText)
        {
            Print("[DZTickets] Ticket menu layout is incomplete.");
            return layoutRoot;
        }

        layoutRoot.FindAnyWidget("ButtonMeasure").SetAlpha(0);
        languagePicker.AddItem("Українська");
        languagePicker.AddItem("English");
        languagePicker.AddItem("Deutsch");
        languagePicker.SetCurrentItem(DZTLocale.Language());
        layoutRoot.FindAnyWidget("CloseConfirm").Show(false);
        menuReady = true;
        owner.SetText("");
        categories.Insert("Скарга");
        categories.Insert("Баг");
        categories.Insert("Підозра на чітера");
        categories.Insert("Інше");
        ResizeToViewport();
        ApplyLanguage();
        AdminControls(false);
        SetCompose(true);
        return layoutRoot;
    }

    void ~DZTMenu() { DZTBus.Client.Remove(Receive); }

    override void OnShow()
    {
        super.OnShow();
        if (!menuReady) return;
        menuVisible = true;
        DZTBus.Client.Insert(Receive);
        RefreshList();
        GetGame().GetMission().AddActiveInputExcludes({"menu"});
    }
    override void OnHide()
    {
        super.OnHide();
        menuVisible = false;
        DZTBus.Client.Remove(Receive);
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(RefreshConversation);
        GetGame().GetMission().RemoveActiveInputExcludes({"menu"}, true);
    }
    override void Update(float timeslice)
    {
        super.Update(timeslice);
        if (!menuReady)
        {
            if (GetUApi().GetInputByID(UAUIBack).LocalPress()) Close();
            return;
        }
        if (ResizeToViewport())
        {
            RenderTicketRows();
            if (!composing && threadResponse) RenderThread(threadResponse);
        }
        UpdateSendState();
        headerElapsed += timeslice;
        if (headerElapsed >= 1.0) { headerElapsed = 0; UpdateHeader(); }
        if (refreshDelay > 0) refreshDelay -= timeslice;
        if (menuVisible && refreshPending && refreshDelay <= 0 && !waiting && !composing && !selectingThread && !viewingOlder && !closingTicket)
        {
            refreshPending = false;
            RefreshConversation();
        }
        if (GetUApi().GetInputByID(UAUIBack).LocalPress())
        {
            if (closingTicket) ConfirmClosure(false);
            else Close();
        }
        if (waiting)
        {
            waitTime += timeslice;
            if (waitTime > 12)
            {
                waiting = false;
                statusText.SetText(DZTLocale.T("Таймаут. Оновіть список перед повторним надсиланням."));
            }
        }
    }

    int Pixels(float value)
    {
        return Math.Round(value * uiScale);
    }

    void SetFont(string name, int size, bool bold = false)
    {
        TextWidget widget = TextWidget.Cast(layoutRoot.FindAnyWidget(name));
        if (!widget) return;
        widget.SetItalic(false);
        if (MultilineEditBoxWidget.Cast(widget))
        {
            // Native edit caret uses the bitmap font's original advances.
            // Scaling only the drawn glyphs separates it from the insertion point.
            widget.SetTextExactSize(0);
            widget.SetTextSpacing(0, 0);
            widget.SetBold(false);
        }
        else
        {
            widget.SetTextExactSize(Pixels(size));
            widget.SetBold(bold);
        }
    }

    bool ResizeToViewport()
    {
        int width, height;
        GetScreenSize(width, height);
        if (width <= 0 || height <= 0 || (width == viewportWidth && height == viewportHeight)) return false;
        viewportWidth = width;
        viewportHeight = height;
        uiScale = Math.Max(1.0, Math.Min(width / 1920.0, height / 1080.0));
        uiScale = Math.Min(uiScale, Math.Min((width - DZTDesign.ScreenMargin * 2.0) / DZTDesign.WindowWidth, (height - DZTDesign.ScreenMargin * 2.0) / DZTDesign.WindowHeight));
        int windowWidth = Pixels(DZTDesign.WindowWidth);
        int windowHeight = Pixels(DZTDesign.WindowHeight);
        layoutRoot.SetScreenSize(windowWidth, windowHeight);
        layoutRoot.SetScreenPos((width - windowWidth) * 0.5, (height - windowHeight) * 0.5);
        SetFont("Title", DZTDesign.WindowTitle, true);
        SetFont("ThreadTitle", DZTDesign.SectionTitle, true);
        SetFont("ComposeTitle", DZTDesign.SectionTitle, true);
        SetFont("Subtitle", 12);
        SetFont("ThreadMeta", DZTDesign.Caption);
        SetFont("TechnicalInfo", DZTDesign.Caption);
        SetFont("ReplyCounter", DZTDesign.Caption);
        SetFont("ThreadStatus", 12, true);
        SetFont("Status", DZTDesign.Caption);
        SetFont("Version", DZTDesign.Caption);
        SetFont("Counter", 12);
        SetFont("ListLabel", DZTDesign.SectionTitle, true);
        SetFont("EmptyList", 14);
        SetFont("ConfirmationCaption", 14);
        SetFont("CategoryLabel", DZTDesign.Caption);
        SetFont("BodyLabel", DZTDesign.Caption);
        SetFont("ReplyLabel", DZTDesign.Caption);
        SetFont("Description", 15);
        SetFont("ReplyDescription", 15);
        SetFont("ThreadSelection", DZTDesign.Body);
        SetFont("CloseTitle", DZTDesign.SectionTitle, true);
        SetFont("CloseExplanation", DZTDesign.Body);
        SetFont("OwnerLabel", DZTDesign.Caption);
        layoutRoot.Update();
        SizeButtons();
        StyleButtons(layoutRoot);
        UpdateTools();
        return true;
    }

    void SetCaption(string name, string caption)
    {
        Widget control = layoutRoot.FindAnyWidget(name);
        TextWidget label = TextWidget.Cast(control.FindAnyWidget(name + "Caption"));
        if (!label)
        {
            label = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets("DZTickets/gui/button_caption.layout", control));
            label.SetName(name + "Caption");
        }
        label.SetText(caption);
        label.SetTextExactSize(Pixels(14));
        if (name == "Confirmation")
        {
            label.SetPos(0.047120419, 0);
            label.SetSize(0.952879581, 1);
            label.ClearFlags(WidgetFlags.CENTER);
        }
    }

    void StyleButtons(Widget parent)
    {
        Widget child = parent.GetChildren();
        while (child)
        {
            if (ButtonWidget.Cast(child) && child.GetName() != "TicketRow")
            {
                ButtonWidget.Cast(child).SetText("");
                TextWidget label = TextWidget.Cast(child.FindAnyWidget(child.GetName() + "Caption"));
                if (child.GetName() == "Exit")
                {
                    SetCaption("Exit", "X");
                    label = TextWidget.Cast(child.FindAnyWidget("ExitCaption"));
                    label.SetTextExactSize(Pixels(18));
                }
                else if (label) label.SetTextExactSize(Pixels(14));
                float bw, bh;
                child.GetScreenSize(bw, bh);
                for (int i = 0; i < 4; i++)
                {
                    Widget edge = child.FindAnyWidget(child.GetName() + "Edge" + i.ToString());
                    if (!edge || bw <= 0 || bh <= 0) continue;
                    float thickness = Math.Max(1, Pixels(1));
                    if (i == 0) { edge.SetPos(0, 0); edge.SetSize(1, thickness / bh); }
                    if (i == 1) { edge.SetPos(0, 1 - thickness / bh); edge.SetSize(1, thickness / bh); }
                    if (i == 2) { edge.SetPos(0, 0); edge.SetSize(thickness / bw, 1); }
                    if (i == 3) { edge.SetPos(1 - thickness / bw, 0); edge.SetSize(thickness / bw, 1); }
                }
            }
            StyleButtons(child);
            child = child.GetSibling();
        }
    }

    int RequiredButtonWidth(string caption, int minimum = 100)
    {
        TextWidget measure = TextWidget.Cast(layoutRoot.FindAnyWidget("ButtonMeasure"));
        measure.SetTextExactSize(Pixels(14));
        array<string> labels = {caption, DZTLocale.English(caption), DZTLocale.German(caption)};
        int maximum;
        foreach (string label : labels)
        {
            measure.SetText(label);
            int width, height;
            measure.GetTextSize(width, height);
            maximum = Math.Max(maximum, width);
        }
        return Math.Max(Pixels(minimum), maximum + Pixels(32));
    }

    void PlaceButton(string name, int x, int width, bool fromRight = false)
    {
        Widget button = layoutRoot.FindAnyWidget(name);
        float parentWidth, parentHeight, oldX, oldY, oldWidth, oldHeight;
        button.GetParent().GetScreenSize(parentWidth, parentHeight);
        if (parentWidth <= 0) return;
        button.GetPos(oldX, oldY);
        button.GetSize(oldWidth, oldHeight);
        float left = x;
        if (fromRight) left = parentWidth - x - width;
        button.SetPos(left / parentWidth, oldY);
        button.SetSize(width / parentWidth, oldHeight);
    }

    void SizeButtons()
    {
        int gap = Pixels(DZTDesign.ButtonGap);
        int inset = Pixels(DZTDesign.PanelInset);
        PlaceButton("Open", 0, RequiredButtonWidth("Відкрити"));
        PlaceButton("Mine", inset, RequiredButtonWidth("Оновити"));
        PlaceButton("SidebarToolsToggle", inset, RequiredButtonWidth("Додатково"), true);
        PlaceButton("ThreadToolsToggle", inset, RequiredButtonWidth("Дії"), true);
        PlaceButton("OlderTickets", 0, RequiredButtonWidth("Раніші звернення"));
        PlaceButton("AllOpen", 0, RequiredButtonWidth("Усі відкриті / Адмін"));
        int historyWidth = RequiredButtonWidth("Історія");
        PlaceButton("History", 0, historyWidth);
        float filterWidth, filterHeight;
        layoutRoot.FindAnyWidget("AdminFilter").GetScreenSize(filterWidth, filterHeight);

        int latestWidth = RequiredButtonWidth("Оновити");
        PlaceButton("Latest", 0, latestWidth);
        PlaceButton("OlderMessages", latestWidth + gap, RequiredButtonWidth("Раніші"));
        int copyWidth = RequiredButtonWidth("Копіювати");
        PlaceButton("CopyThread", 0, copyWidth, true);
        PlaceButton("SelectThread", copyWidth + gap, Math.Max(RequiredButtonWidth("Виділити"), RequiredButtonWidth("Перегляд")), true);
        int adminWidth = Math.Max(RequiredButtonWidth("Взяти в роботу"), RequiredButtonWidth("Закрити тікет"));
        PlaceButton("Claim", 0, adminWidth);
        PlaceButton("Resolve", adminWidth + gap, adminWidth);
        int replyWidth = RequiredButtonWidth("Відповісти", 160);
        PlaceButton("Reply", inset, replyWidth, true);
        float panelWidth, panelHeight;
        layoutRoot.FindAnyWidget("ThreadPanel").GetScreenSize(panelWidth, panelHeight);
        if (panelWidth > 0)
        {
            float inputWidth = panelWidth - inset * 2 - gap - replyWidth;
            layoutRoot.FindAnyWidget("ReplyInput").SetSize(inputWidth / panelWidth, 104.0 / DZTDesign.PanelHeight);
            layoutRoot.FindAnyWidget("ReplyCounter").SetPos((panelWidth - inset - replyWidth) / panelWidth, 480.0 / DZTDesign.PanelHeight);
            layoutRoot.FindAnyWidget("ReplyCounter").SetSize(replyWidth / panelWidth, 20.0 / DZTDesign.PanelHeight);
        }
        PlaceButton("Create", inset, RequiredButtonWidth("Надіслати звернення"), true);
        int confirmWidth = RequiredButtonWidth("Так, закрити");
        PlaceButton("ConfirmClose", Pixels(24), confirmWidth, true);
        PlaceButton("CancelClose", Pixels(24) + confirmWidth + gap, RequiredButtonWidth("Скасувати"), true);
    }

    void ApplyLanguage()
    {
        applyingLanguage = true;
        languagePicker.SetCurrentItem(DZTLocale.Language());
        SetCaption("SidebarToolsToggle", DZTLocale.T("Додатково"));
        SetCaption("ThreadToolsToggle", DZTLocale.T("Дії"));
        SetCaption("CancelClose", DZTLocale.T("Скасувати"));
        SetCaption("ConfirmClose", DZTLocale.T("Так, закрити"));
        TextWidget.Cast(layoutRoot.FindAnyWidget("CloseTitle")).SetText(DZTLocale.T("Закрити це звернення?"));
        TextWidget.Cast(layoutRoot.FindAnyWidget("CloseExplanation")).SetText(DZTLocale.T("Після закриття відповіді будуть недоступні. Історія листування збережеться."));
        TextWidget.Cast(layoutRoot.FindAnyWidget("Title")).SetText(DZTLocale.T("Підтримка"));
        TextWidget.Cast(layoutRoot.FindAnyWidget("Version")).SetText("DayZ Ticket System v" + DZTDesign.Version);
        UpdateHeader();
        SetCaption("NewTicket", DZTLocale.T("+ Нове звернення"));
        TextWidget.Cast(layoutRoot.FindAnyWidget("ListLabel")).SetText(DZTLocale.T("Мої звернення"));
        TextWidget.Cast(layoutRoot.FindAnyWidget("EmptyList")).SetText(DZTLocale.T("Немає звернень"));
        SetCaption("SelectThread", DZTLocale.T("Виділити"));
        SetCaption("CopyThread", DZTLocale.T("Копіювати"));
        SetCaption("Open", DZTLocale.T("Відкрити"));
        SetCaption("Mine", DZTLocale.T("Оновити"));
        SetCaption("OlderTickets", DZTLocale.T("Раніші звернення"));
        SetCaption("AllOpen", DZTLocale.T("Усі відкриті / Адмін"));
        SetCaption("History", DZTLocale.T("Історія"));
        TextWidget.Cast(layoutRoot.FindAnyWidget("ComposeTitle")).SetText(DZTLocale.T("Нове звернення"));
        TextWidget.Cast(layoutRoot.FindAnyWidget("CategoryLabel")).SetText(DZTLocale.T("Категорія"));
        TextWidget.Cast(layoutRoot.FindAnyWidget("BodyLabel")).SetText(DZTLocale.T("Опис"));
        SetCaption("Confirmation", DZTLocale.T("Підтвердити надсилання"));
        SetCaption("Create", DZTLocale.T("Надіслати звернення"));
        SetCaption("Latest", DZTLocale.T("Оновити"));
        SetCaption("OlderMessages", DZTLocale.T("Раніші"));
        TextWidget.Cast(layoutRoot.FindAnyWidget("ReplyLabel")).SetText(DZTLocale.T("Ваша відповідь"));
        SetCaption("Claim", DZTLocale.T("Взяти в роботу"));
        SetCaption("Resolve", DZTLocale.T("Закрити тікет"));
        SetCaption("Reply", DZTLocale.T("Відповісти"));
        int index = category.GetCurrentItem();
        category.ClearAll();
        foreach (string label : categories) category.AddItem(DZTLocale.T(label));
        if (index >= 0 && index < categories.Count()) category.SetCurrentItem(index);
        else if (categories.Count() > 0) category.SetCurrentItem(0);
        RenderTicketRows();
        statusText.SetText("");
        if (!composing && threadResponse)
        {
            RenderThread(threadResponse);
            if (selectingThread) threadSelection.SetText(clipboardText);
        }
        UpdateSelectionLabel();
        SizeButtons();
        StyleButtons(layoutRoot);
        applyingLanguage = false;
    }

    string ClockPart(int value)
    {
        if (value < 10) return "0" + value.ToString();
        return value.ToString();
    }

    void UpdateHeader()
    {
        if (!menuReady || !GetGame().GetWorld()) return;
        int year, month, day, hour, minute;
        GetGame().GetWorld().GetDate(year, month, day, hour, minute);
        string line = DZTLocale.T("Час гри: ") + ClockPart(day) + "." + ClockPart(month) + "." + year.ToString() + " " + ClockPart(hour) + ":" + ClockPart(minute);
        if (isAdmin)
        {
            string mode = DZTLocale.T("Мої");
            if (allOpen) mode = DZTLocale.T("Усі відкриті");
            if (historyOwner != "") mode = DZTLocale.T("Історія");
            line += " | Admin | " + mode + " | " + DZTLocale.T("У списку: ") + tickets.Count().ToString();
            if (!composing && selectedTicket > 0) line += " | #" + selectedTicket.ToString();
        }
        if (line == lastHeaderText) return;
        lastHeaderText = line;
        TextWidget.Cast(layoutRoot.FindAnyWidget("Subtitle")).SetText(line);
    }

    void AdminControls(bool show)
    {
        isAdmin = show;
        layoutRoot.FindAnyWidget("AdminFilter").Show(show);
        UpdateTools();
    }

    void UpdateTools()
    {
        if (!menuReady) return;
        layoutRoot.FindAnyWidget("SidebarTools").Show(sidebarToolsOpen);
        ticketList.Show(!sidebarToolsOpen);
        layoutRoot.FindAnyWidget("EmptyList").Show(!sidebarToolsOpen && tickets.Count() == 0);
        layoutRoot.FindAnyWidget("ThreadTools").Show(threadToolsOpen);
        layoutRoot.FindAnyWidget("TechnicalInfo").Show(threadToolsOpen && isAdmin);
        int top = DZTDesign.ConversationTop;
        if (threadToolsOpen) top += DZTDesign.ToolsHeight + DZTDesign.ToolsGap;
        int bottom = DZTDesign.ConversationBottom;
        if (ticketClosed) bottom = DZTDesign.ClosedConversationBottom;
        float fractionWidth = DZTDesign.ThreadContentWidth / (DZTDesign.ThreadWidth * 1.0);
        float left = DZTDesign.ThreadInset / (DZTDesign.ThreadWidth * 1.0);
        float y = top / (DZTDesign.PanelHeight * 1.0);
        float height = (bottom - top) / (DZTDesign.PanelHeight * 1.0);
        conversationScroll.SetPos(left, y);
        conversationScroll.SetSize(fractionWidth, height);
        Widget selection = layoutRoot.FindAnyWidget("ThreadSelectionPanel");
        selection.SetPos(left, y);
        selection.SetSize(fractionWidth, height);
        // Inner padding remains physical pixels after either panel is resized.
        float selectionWidth, selectionHeight;
        selection.GetScreenSize(selectionWidth, selectionHeight);
        if (selectionWidth > 0 && selectionHeight > 0)
        {
            float pad = Pixels(DZTDesign.MessagePadding);
            threadSelection.SetPos(pad / selectionWidth, pad / selectionHeight);
            threadSelection.SetSize(1 - 2 * pad / selectionWidth, 1 - 2 * pad / selectionHeight);
        }
        PaintButton(layoutRoot.FindAnyWidget("SidebarToolsToggle"));
        PaintButton(layoutRoot.FindAnyWidget("ThreadToolsToggle"));
    }

    void ConfirmClosure(bool show)
    {
        bool wasClosing = closingTicket;
        closingTicket = show;
        layoutRoot.FindAnyWidget("CloseConfirm").Show(show);
        if (show) SetFocus(layoutRoot.FindAnyWidget("CancelClose"));
        else if (wasClosing) SetFocus(layoutRoot.FindAnyWidget("ThreadToolsToggle"));
    }

    void EnableControl(string name, bool enabled)
    {
        Widget control = layoutRoot.FindAnyWidget(name);
        if (controlStates.Contains(name) && controlStates.Get(name) == enabled) return;
        control.Enable(enabled);
        controlStates.Set(name, enabled);
        PaintButton(control);
    }

    void PaintButton(Widget control)
    {
        if (!control) return;
        string name = control.GetName();
        Widget background = control.FindAnyWidget(name + "Background");
        if (!background) return;
        int color = DZTDesign.ElevatedSurface;
        if ((name == "SidebarToolsToggle" && sidebarToolsOpen) || (name == "ThreadToolsToggle" && threadToolsOpen)) color = DZTDesign.Selected;
        if (name == "NewTicket" || name == "Create" || name == "Reply") color = DZTDesign.PrimaryAction;
        bool danger = name == "Exit" || name == "Resolve" || name == "ConfirmClose";
        if (danger) color = DZTDesign.Danger;
        bool enabled = true;
        if (controlStates.Contains(name)) enabled = controlStates.Get(name);
        if (enabled && control == hoveredButton) color = DZTDesign.Selected;
        if (enabled && control == pressedButton) color = DZTDesign.Background;
        if (danger && enabled && control == hoveredButton) color = DZTDesign.DangerHover;
        if (danger && enabled && control == pressedButton) color = DZTDesign.DangerPressed;
        background.SetColor(color);
        TextWidget label = TextWidget.Cast(control.FindAnyWidget(name + "Caption"));
        if (label)
        {
            if (enabled) label.SetColor(DZTDesign.TextPrimary);
            else label.SetColor(DZTDesign.TextSecondary);
        }
        if (enabled) background.SetAlpha(1);
        else background.SetAlpha(0.45);
    }

    string StatusLabel(string value)
    {
        if (value == "open") return DZTLocale.T("Нове");
        if (value == "in_progress") return DZTLocale.T("У роботі");
        if (value == "closed") return DZTLocale.T("Закрите");
        return value;
    }

    void SetCompose(bool value)
    {
        composing = value;
        if (value) { ticketClosed = false; selectedRow = -1; HighlightTicketRows(); }
        ConfirmClosure(false);
        SetThreadSelection(false);
        layoutRoot.FindAnyWidget("ComposePanel").Show(value);
        layoutRoot.FindAnyWidget("ThreadPanel").Show(!value);
        if (value) description = MultilineEditBoxWidget.Cast(layoutRoot.FindAnyWidget("Description"));
        else description = MultilineEditBoxWidget.Cast(layoutRoot.FindAnyWidget("ReplyDescription"));
        confirmation.SetChecked(false);
        UpdateTools();
        UpdateSendState();
    }

    void UpdateSendState()
    {
        if (!menuReady || !description || !counter) return;
        string body;
        description.GetText(body);
        body.TrimInPlace();
        int length = body.LengthUtf8();
        bool valid = length > 0 && length <= 2000 && !waiting;
        EnableControl("Create", valid && confirmation.IsChecked() && categories.Count() > 0 && serverCategories);
        int row = selectedRow;
        EnableControl("Open", !waiting && row >= 0 && row < tickets.Count());
        EnableControl("OlderTickets", !waiting && listMore);
        EnableControl("OlderMessages", !waiting && messageMore);
        EnableControl("SelectThread", !waiting && clipboardText != "");
        EnableControl("CopyThread", !waiting && clipboardText != "");
        layoutRoot.FindAnyWidget("ReplyInput").Show(!ticketClosed);
        layoutRoot.FindAnyWidget("ReplyLabel").Show(!ticketClosed);
        EnableControl("Claim", !refreshPending && !waiting && !ticketClosed && threadResponse && threadResponse.tickets.Count() > 0 && threadResponse.tickets[0].assignee == "");
        EnableControl("Resolve", !refreshPending && !waiting && !ticketClosed);
        EnableControl("Reply", !refreshPending && valid && selectedTicket > 0 && !ticketClosed);
        layoutRoot.FindAnyWidget("AdminActions").Show(isAdmin && !ticketClosed && !composing);
        layoutRoot.FindAnyWidget("Reply").Show(!ticketClosed);
        EnableControl("NewTicket", !waiting);
        ticketList.Enable(!waiting);
        EnableControl("Mine", !waiting);
        EnableControl("AllOpen", !waiting);
        EnableControl("History", !waiting);
        EnableControl("Latest", !waiting && selectedTicket > 0);
        counter.SetText(length.ToString() + " / 2000");
        TextWidget replyCounter = TextWidget.Cast(layoutRoot.FindAnyWidget("ReplyCounter"));
        replyCounter.SetText(length.ToString() + " / 2000");
        replyCounter.Show(!composing && !ticketClosed);
        if (length > 2000) replyCounter.SetColor(DZTDesign.ErrorText);
        else replyCounter.SetColor(DZTDesign.TextSecondary);
        if (length > 2000) counter.SetColor(DZTDesign.ErrorText);
        else counter.SetColor(DZTDesign.TextSecondary);
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        if (!menuReady || applyingLanguage || normalizingInput) return super.OnChange(w, x, y, finished);
        if (w == description)
        {
            string typed;
            description.GetText(typed);
            string readable = DZTLocale.Readable(typed);
            if (readable != typed)
            {
                normalizingInput = true;
                description.SetText(readable);
                normalizingInput = false;
            }
        }
        if (w == languagePicker)
        {
            DZTLocale.Select(languagePicker.GetCurrentItem());
            ApplyLanguage();
            return true;
        }
        if (w == ticketList)
        {
            selectedRow = ticketList.GetSelectedRow();
            UpdateSendState();
            return true;
        }
        if (w == description || w == category) confirmation.SetChecked(false);
        UpdateSendState();
        return super.OnChange(w, x, y, finished);
    }

    void Send(DZTRequest req)
    {
        if (waiting) return;
        bool mutation = req.op == "create" || req.op == "reply" || req.op == "claim" || req.op == "close";
        req.request_id = Math.RandomInt(1, 2147483646).ToString() + "-" + Math.RandomInt(1, 2147483646).ToString() + "-" + GetGame().GetTime().ToString();
        if (req.op == "create") req.ticket_id = 0;
        req.body.TrimInPlace();
        if (req.op == "create" || req.op == "reply")
        {
            submittedInput = description;
            description.GetText(submittedBody);
        }
        if (mutation)
        {
            if (retryMutation && retryMutation.op == req.op && retryMutation.ticket_id == req.ticket_id && retryMutation.category == req.category && retryMutation.body == req.body)
                req.request_id = retryMutation.request_id;
            retryMutation = req;
        }
        expectedRequest = req.request_id;
        waiting = true;
        waitTime = 0;
        statusText.SetColor(DZTDesign.TextSecondary);
        statusText.SetText(DZTLocale.T("Завантаження..."));
        Print("[DZTickets] Sending request: " + req.op);
        DZTBus.Send(req);
    }

    void RefreshList(bool older = false)
    {
        DZTRequest req = new DZTRequest;
        req.op = "list";
        req.all_open = allOpen;
        if (historyOwner != "")
        {
            req.op = "history";
            req.owner = historyOwner;
        }
        if (older) req.before = listBefore;
        Send(req);
    }

    void Detail(bool older = false, int ticketId = 0)
    {
        if (ticketId == 0) ticketId = selectedTicket;
        if (ticketId <= 0) return;
        viewingOlder = older;
        DZTRequest req = new DZTRequest;
        req.op = "detail";
        req.ticket_id = ticketId;
        if (older) req.before = messageBefore;
        Send(req);
    }

    void Receive(int rpc, string json)
    {
        if (rpc == DZT_RPC_NOTIFY)
        {
            DZTMessage notification = new DZTMessage;
            string notificationError;
            if (JsonFileLoader<DZTMessage>.LoadData(json, notification, notificationError) && !composing && notification.ticket == selectedTicket)
            {
                refreshPending = true;
                refreshDelay = 0.7;
            }
            return;
        }
        if (rpc != DZT_RPC_RESPONSE) return;
        DZTResponse response = new DZTResponse;
        string error;
        if (!JsonFileLoader<DZTResponse>.LoadData(json, response, error)) return;
        if (response.request_id != expectedRequest) return;
        Print("[DZTickets] Response received: " + response.op + " tickets=" + response.tickets.Count().ToString());
        expectedRequest = "";
        if (retryMutation && retryMutation.request_id == response.request_id)
            retryMutation = null;
        waiting = false;
        refreshDelay = 0.7;
        if (!response.ok)
        {
            statusText.SetColor(DZTDesign.ErrorText);
            statusText.SetText(DZTLocale.T(response.error));
            return;
        }
        AdminControls(response.admin);
        if (response.categories && (!composing || categories.Count() == 0 || !serverCategories))
        {
            int selectedCategory = category.GetCurrentItem();
            categories = response.categories;
            category.ClearAll();
            foreach (string label : categories) category.AddItem(DZTLocale.T(label));
            if (selectedCategory < 0 || selectedCategory >= categories.Count()) selectedCategory = 0;
            if (categories.Count() > 0) category.SetCurrentItem(selectedCategory);
        }
        serverCategories = true;
        if (response.op == "list" || response.op == "history")
        {
            if (!composing && selectedTicket > 0)
            {
                string oldReply;
                description.GetText(oldReply);
                replyDrafts.Set(selectedTicket, oldReply);
            }
            tickets = response.tickets;
            selectedRow = -1;
            RenderTicketRows();
            if (tickets.Count() > 0) ticketList.EnsureVisible(0);
            listBefore = response.next_before;
            listMore = response.has_more;
            selectedTicket = 0;
            clipboardText = "";
            threadResponse = null;
            ClearMessageCards();
            SetCompose(true);
            layoutRoot.FindAnyWidget("EmptyList").Show(tickets.Count() == 0);
            statusText.SetText("");
        }
        else if (response.op == "detail")
        {
            if (response.tickets.Count() == 0) return;
            bool changedTicket = selectedTicket != response.ticket_id;
            if (changedTicket && !composing && selectedTicket > 0)
            {
                string oldDraft;
                description.GetText(oldDraft);
                replyDrafts.Set(selectedTicket, oldDraft);
            }
            if (changedTicket)
            {
                string restoredDraft = "";
                if (replyDrafts.Contains(response.ticket_id)) restoredDraft = replyDrafts.Get(response.ticket_id);
                MultilineEditBoxWidget.Cast(layoutRoot.FindAnyWidget("ReplyDescription")).SetText(restoredDraft);
            }
            scrollToLatest = scrollToLatest || selectedTicket != response.ticket_id;
            selectedTicket = response.ticket_id;
            int found = -1;
            for (int ticketIndex = 0; ticketIndex < tickets.Count(); ticketIndex++)
                if (tickets[ticketIndex].id == selectedTicket) found = ticketIndex;
            if (found < 0)
            {
                tickets.InsertAt(response.tickets[0], 0);
                found = 0;
            }
            else tickets[found] = response.tickets[0];
            selectedRow = found;
            RenderTicketRows();
            layoutRoot.FindAnyWidget("EmptyList").Show(false);
            threadResponse = response;
            messageBefore = response.next_before;
            messageMore = response.has_more;
            ticketClosed = response.tickets[0].status == "closed";
            SetCompose(false);
            RenderThread(response);
        }
        else
        {
            if ((response.op == "create" || response.op == "reply") && submittedInput)
            {
                string currentBody;
                submittedInput.GetText(currentBody);
                if (currentBody == submittedBody) submittedInput.SetText("");
                submittedInput = null;
            }
            confirmation.SetChecked(false);
            selectedTicket = response.ticket_id;
            if (response.op == "create")
                MultilineEditBoxWidget.Cast(layoutRoot.FindAnyWidget("ReplyDescription")).SetText("");
            if (response.op == "reply")
            {
                string remainingDraft;
                MultilineEditBoxWidget.Cast(layoutRoot.FindAnyWidget("ReplyDescription")).GetText(remainingDraft);
                replyDrafts.Set(response.ticket_id, remainingDraft);
            }
            SetCompose(false);
            scrollToLatest = true;
            viewingOlder = false;
            statusText.SetText(DZTLocale.T("Звернення #") + response.ticket_id.ToString() + DZTLocale.T(" збережено."));
            refreshPending = true;
            refreshDelay = 0.7;
        }
    }

    void ClearMessageCards()
    {
        Widget child = messageStack.GetChildren();
        while (child)
        {
            Widget next = child.GetSibling();
            child.Unlink();
            child = next;
        }
    }

    void RenderThread(DZTResponse response)
    {
        float previousScroll = conversationScroll.GetVScrollPos01();
        ClearMessageCards();
        DZTTicket ticket = response.tickets[0];
        string heading = DZTLocale.T("Звернення #") + ticket.id.ToString();
        TextWidget.Cast(layoutRoot.FindAnyWidget("ThreadStatus")).SetText(StatusLabel(ticket.status));
        string meta = DZTLocale.T(ticket.category) + "  ·  " + ticket.created;
        if (response.admin)
        {
            TextWidget.Cast(layoutRoot.FindAnyWidget("TechnicalInfo")).SetText("SteamID64: " + ticket.steam + "   XYZ: " + ticket.position);
            owner.SetText(ticket.steam);
        }
        threadTitle.SetText(heading);
        threadMeta.SetText(meta);
        clipboardText = heading + " · " + StatusLabel(ticket.status) + "\n" + meta + "\n" + ticket.name + "\n" + ticket.created + "\n\n";
        if (response.admin) clipboardText += "SteamID64: " + ticket.steam + "   XYZ: " + ticket.position + "\n\n";
        int y = Pixels(DZTDesign.MessagePadding);
        foreach (DZTMessage msg : response.messages)
        {
            bool player = msg.actor == ticket.steam;
            string author = DZTLocale.T("Підтримка");
            if (player) author = ticket.name;
            if (msg.actor_name != "") author = msg.actor_name;
            string body = msg.body;
            if (msg.kind == "status") body = DZTLocale.T("Статус: ") + StatusLabel(msg.body);
            clipboardText += author + "  ·  " + msg.created + "\n" + body + "\n\n";
            y += AddMessageCard(author, msg.created, body, y, player);
        }
        messageStack.SetSize(0.973, y + Pixels(DZTDesign.MessagePadding));
        if (viewingOlder) conversationScroll.VScrollToPos01(0);
        else if (scrollToLatest || previousScroll > 0.95) conversationScroll.VScrollToPos01(1);
        else conversationScroll.VScrollToPos01(previousScroll);
        scrollToLatest = false;
        statusText.SetText("");
    }

    int AddMessageCard(string author, string date, string body, int y, bool player)
    {
        Widget card = GetGame().GetWorkspace().CreateWidgets("DZTickets/gui/ticket_message.layout", messageStack);
        if (!card) return 0;
        float stackWidth, stackHeight;
        messageStack.GetScreenSize(stackWidth, stackHeight);
        if (stackWidth <= 0) { card.Unlink(); return 0; }
        float x = Pixels(DZTDesign.MessagePadding) / stackWidth;
        if (player) x = 0.15 - Pixels(DZTDesign.MessagePadding) / stackWidth;
        card.SetPos(x, y);
        card.SetSize(0.85, Pixels(DZTDesign.MessageMinHeight));
        if (player) card.SetColor(DZTDesign.PlayerMessage);
        else card.SetColor(DZTDesign.ElevatedSurface);
        MultilineTextWidget authorWidget = MultilineTextWidget.Cast(card.FindAnyWidget("MessageAuthor"));
        TextWidget dateWidget = TextWidget.Cast(card.FindAnyWidget("MessageDate"));
        float padding = Pixels(DZTDesign.MessagePadding) / (stackWidth * 0.85);
        authorWidget.SetPos(padding, Pixels(DZTDesign.MessagePadding));
        dateWidget.SetPos(0.60, Pixels(DZTDesign.MessagePadding));
        authorWidget.SetSize(0.56 - padding, Pixels(200));
        dateWidget.SetSize(0.40 - padding, Pixels(DZTDesign.MessageHeaderHeight));
        authorWidget.SetTextExactSize(Pixels(DZTDesign.Caption));
        dateWidget.SetTextExactSize(Pixels(DZTDesign.Caption));
        authorWidget.SetBold(true);
        authorWidget.SetText(DZTLocale.Readable(author));
        dateWidget.SetText(date);
        card.Update();
        int authorWidth, authorHeight;
        authorWidget.GetTextSize(authorWidth, authorHeight);
        authorHeight = Math.Max(authorHeight, Pixels(DZTDesign.MessageHeaderHeight));
        authorWidget.SetSize(0.56 - padding, authorHeight);
        MultilineTextWidget content = MultilineTextWidget.Cast(card.FindAnyWidget("MessageBody"));
        int bodyTop = Pixels(DZTDesign.MessagePadding + DZTDesign.MessageHeaderGap) + authorHeight;
        content.SetPos(padding, bodyTop);
        content.SetTextExactSize(Pixels(DZTDesign.Body));
        content.SetTextSpacing(0, Pixels(DZTDesign.MessageTextLeading));
        content.SetSize(1 - 2 * padding, Pixels(20000));
        content.SetText(DZTLocale.Readable(body));
        card.Update();
        int width, height;
        content.GetTextSize(width, height);
        height = Math.Max(height, Pixels(DZTDesign.Body));
        int cardHeight = Math.Max(Pixels(DZTDesign.MessageMinHeight), bodyTop + height + Pixels(DZTDesign.MessagePadding));
        content.SetSize(1 - 2 * padding, height);
        card.SetSize(0.85, cardHeight);
        return cardHeight + Pixels(DZTDesign.MessageGap);
    }

    void RefreshConversation()
    {
        if (!waiting && !composing && !selectingThread && !closingTicket) Detail();
    }

    void SetThreadSelection(bool selecting)
    {
        selectingThread = selecting;
        conversationScroll.Show(!selecting);
        layoutRoot.FindAnyWidget("ThreadSelectionPanel").Show(selecting);
        if (selecting)
        {
            threadSelection.SetText(clipboardText);
            SetFocus(threadSelection);
        }
        UpdateSelectionLabel();
    }

    void UpdateSelectionLabel()
    {
        string label = "Виділити";
        if (selectingThread) label = "Перегляд";
        SetCaption("SelectThread", DZTLocale.T(label));
    }

    void RenderTicketRows()
    {
        if (!ticketList) return;
        ticketList.ClearItems();
        foreach (DZTTicket ticket : tickets)
        {
            string prefix = "#" + ticket.id.ToString() + "  ";
            string suffix = "  ·  " + StatusLabel(ticket.status);
            string categoryLabel = DZTLocale.T(ticket.category);
            string label = prefix + categoryLabel + suffix;
            TextWidget measure = TextWidget.Cast(layoutRoot.FindAnyWidget("ButtonMeasure"));
            measure.SetTextExactSize(Pixels(DZTDesign.Button));
            float available, listHeight;
            ticketList.GetScreenSize(available, listHeight);
            // Keep the number and status intact; long category data is an explicit preview.
            available -= Pixels(32);
            int measuredWidth, measuredHeight;
            measure.SetText(label);
            measure.GetTextSize(measuredWidth, measuredHeight);
            int categoryLength = categoryLabel.LengthUtf8();
            while (available > 0 && measuredWidth > available && categoryLength > 0)
            {
                categoryLength--;
                label = prefix + categoryLabel.SubstringUtf8(0, categoryLength) + "…" + suffix;
                measure.SetText(label);
                measure.GetTextSize(measuredWidth, measuredHeight);
            }
            ticketList.AddItem(label, ticket, 0);
        }
        HighlightTicketRows();
    }

    void HighlightTicketRows()
    {
        if (!ticketList) return;
        ticketList.SelectRow(selectedRow);
        for (int row = 0; row < tickets.Count(); row++)
            ticketList.SetItemColor(row, 0, DZTDesign.TextPrimary);
    }

    bool OpenSelectedTicket()
    {
        int row = selectedRow;
        if (waiting || row < 0 || row >= tickets.Count()) return false;
        scrollToLatest = true;
        Detail(false, tickets[row].id);
        return true;
    }

    override bool OnDoubleClick(Widget w, int x, int y, int button)
    {
        if (w == ticketList && button == MouseState.LEFT)
        {
            selectedRow = ticketList.GetSelectedRow();
            return OpenSelectedTicket();
        }
        return super.OnDoubleClick(w, x, y, button);
    }

    Widget ButtonTarget(Widget target)
    {
        while (target && target != layoutRoot)
        {
            if (ButtonWidget.Cast(target) || target == confirmation) return target;
            target = target.GetParent();
        }
        return null;
    }

    void SetButtonEdges(Widget w, float alpha)
    {
        Widget target = ButtonTarget(w);
        if (!ButtonWidget.Cast(target)) return;
        Widget edge = target.GetChildren();
        while (edge)
        {
            if (edge.GetName().Contains("Edge")) edge.SetAlpha(alpha);
            edge = edge.GetSibling();
        }
    }

    void PaintInput(Widget w, int color)
    {
        if (!w) return;
        Widget field = w.GetParent();
        if (!field) return;
        for (int i = 0; i < 4; i++)
        {
            Widget edge = field.FindAnyWidget(field.GetName() + "Edge" + i.ToString());
            if (edge) edge.SetColor(color);
        }
    }

    override bool OnFocus(Widget w, int x, int y)
    {
        PaintInput(w, DZTDesign.BorderActive);
        SetButtonEdges(w, 1.0);
        return super.OnFocus(w, x, y);
    }

    override bool OnFocusLost(Widget w, int x, int y)
    {
        PaintInput(w, DZTDesign.Border);
        return super.OnFocusLost(w, x, y);
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        hoveredButton = ButtonTarget(w);
        PaintButton(hoveredButton);
        SetButtonEdges(w, 1.0);
        return super.OnMouseEnter(w, x, y);
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (ButtonTarget(w) != ButtonTarget(enterW))
        {
            hoveredButton = null;
            PaintButton(ButtonTarget(w));
            SetButtonEdges(w, 0.78);
        }
        return super.OnMouseLeave(w, enterW, x, y);
    }

    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (button == MouseState.LEFT)
        {
            pressedButton = ButtonTarget(w);
            PaintButton(pressedButton);
            SetButtonEdges(w, 0.56);
        }
        return super.OnMouseButtonDown(w, x, y, button);
    }

    override bool OnMouseButtonUp(Widget w, int x, int y, int button)
    {
        if (button == MouseState.LEFT)
        {
            pressedButton = null;
            PaintButton(ButtonTarget(w));
            SetButtonEdges(w, 1.0);
        }
        return super.OnMouseButtonUp(w, x, y, button);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (!menuReady) return super.OnClick(w, x, y, button);
        if (w == ticketList)
        {
            selectedRow = ticketList.GetSelectedRow();
            return OpenSelectedTicket();
        }
        Widget target = ButtonTarget(w);
        if (!target || target == layoutRoot)
        {
    
            return super.OnClick(w, x, y, button);
        }
        string action = target.GetName();
        if (action == "CancelClose") { ConfirmClosure(false); return true; }
        if (closingTicket && action != "ConfirmClose") return true;
        if (action == "Exit") { Close(); return true; }
        if (action == "SidebarToolsToggle") { sidebarToolsOpen = !sidebarToolsOpen; UpdateTools(); return true; }
        if (action == "ThreadToolsToggle") { threadToolsOpen = !threadToolsOpen; UpdateTools(); return true; }
        if (waiting) return true;
        if (action == "NewTicket")
        {
            if (!composing && selectedTicket > 0)
            {
                string draft;
                description.GetText(draft);
                replyDrafts.Set(selectedTicket, draft);
            }
            selectedTicket = 0;
            refreshPending = false;
            viewingOlder = false;
            SetCompose(true);
            statusText.SetText("");
            return true;
        }
        if (action == "Confirmation") { UpdateSendState(); return true; }
        if (action == "Mine" || action == "AllOpen" || action == "History")
        {
            allOpen = action == "AllOpen";
            historyOwner = "";
            if (action == "History")
            {
                historyOwner = owner.GetText();
                historyOwner.TrimInPlace();
                if (historyOwner.Length() != 17)
                {
                    statusText.SetText(DZTLocale.T("Некоректний SteamID"));
                    return true;
                }
            }
            RefreshList();
            return true;
        }
        if (action == "OlderTickets")
        {
            if (listMore) RefreshList(true);
            return true;
        }
        if (action == "Open") return OpenSelectedTicket();
        if (action == "SelectThread")
        {
            SetThreadSelection(!selectingThread);
            if (selectingThread) statusText.SetText(DZTLocale.T("Виділення тексту"));
            return true;
        }
        if (action == "CopyThread")
        {
            if (clipboardText != "")
            {
                GetGame().CopyToClipboard(clipboardText);
                statusText.SetText(DZTLocale.T("Скопійовано"));
            }
            return true;
        }
        if (action == "Latest") { Detail(); return true; }
        if (action == "OlderMessages")
        {
            if (messageMore) Detail(true);
            return true;
        }
        DZTRequest req = new DZTRequest;
        req.ticket_id = selectedTicket;
        if (action == "Create")
        {
            if (!confirmation.IsChecked())
            {
                statusText.SetText(DZTLocale.T("Поставте галочку підтвердження перед надсиланням."));
                return true;
            }
            int index = category.GetCurrentItem();
            if (index < 0 || index >= categories.Count()) return true;
            req.op = "create";
            req.confirmed = true;
            req.category = categories[index];
            description.GetText(req.body);
        }
        else if (action == "Reply")
        {
            req.op = "reply";
            description.GetText(req.body);
        }
        else if (action == "Claim") req.op = "claim";
        else if (action == "Resolve")
        {
            if (isAdmin && !ticketClosed && selectedTicket > 0) ConfirmClosure(true);
            return true;
        }
        else if (action == "ConfirmClose")
        {
            if (!closingTicket || !isAdmin || ticketClosed) return true;
            ConfirmClosure(false);
            req.op = "close";
        }
        else return super.OnClick(w, x, y, button);
        if (req.op != "create" && selectedTicket <= 0)
        {
            statusText.SetText(DZTLocale.T("Спочатку відкрийте тікет."));
            return true;
        }
        Send(req);
        return true;
    }
}

modded class MissionGameplay
{
    ref array<int> dztSeen = new array<int>;
    bool dztPreview;
    override void OnInit()
    {
        super.OnInit();
        DZTBus.Client.Insert(DZTNotification);
        string preview;
        dztPreview = GetGame().CommandlineGetParam("dztPreview", preview);
    }
    override void OnMissionFinish()
    {
        DZTBus.Client.Remove(DZTNotification);
        super.OnMissionFinish();
    }
    override UIScriptedMenu CreateScriptedMenu(int id)
    {
        if (id == DZT_MENU)
        {
            DZTMenu menu = new DZTMenu;
            menu.SetID(id);
            return menu;
        }
        return super.CreateScriptedMenu(id);
    }
    override void OnUpdate(float timeslice)
    {
        bool requested = GetUApi().GetInputByName("UADZTickets").LocalPress();
        super.OnUpdate(timeslice);
        if (GetGame().GetPlayer() && (requested || dztPreview) && !GetGame().GetUIManager().GetMenu())
        {
            dztPreview = false;
            GetGame().GetUIManager().EnterScriptedMenu(DZT_MENU, null);
        }
    }
    override void OnKeyPress(int key)
    {
        if (key == KeyCode.KC_F7 && GetGame().GetPlayer() && !GetGame().GetUIManager().GetMenu())
        {
            GetGame().GetUIManager().EnterScriptedMenu(DZT_MENU, null);
            return;
        }
        super.OnKeyPress(key);
    }
    void DZTNotification(int rpc, string json)
    {
        if (rpc != DZT_RPC_NOTIFY) return;
        DZTMessage msg = new DZTMessage;
        string error;
        if (!JsonFileLoader<DZTMessage>.LoadData(json, msg, error)) return;
        if (dztSeen.Find(msg.id) == -1)
        {
            NotificationSystem.AddNotificationExtended(8, DZTLocale.T("Тікет #") + msg.ticket.ToString(), DZTLocale.T("Нова відповідь або статус. F7 - відкрити підтримку."));
            dztSeen.Insert(msg.id);
            if (dztSeen.Count() > 256) dztSeen.Remove(0);
        }
        DZTRequest req = new DZTRequest;
        req.op = "ack";
        req.event_id = msg.id;
        DZTBus.Send(req);
    }
}




