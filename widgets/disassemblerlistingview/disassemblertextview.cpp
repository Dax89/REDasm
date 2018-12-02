#include "disassemblertextview.h"
#include "../../models/disassemblermodel.h"
#include <QtWidgets>
#include <QtGui>
#include <cmath>

#define CURSOR_BLINK_INTERVAL 500  // 500ms
#define DOCUMENT_IDEAL_SIZE   10

DisassemblerTextView::DisassemblerTextView(QWidget *parent): QAbstractScrollArea(parent), m_disassembler(NULL), m_disassemblerpopup(NULL)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setStyleHint(QFont::TypeWriter);

    this->setFont(font);
    this->setCursor(Qt::ArrowCursor);
    this->setFrameStyle(QFrame::NoFrame);
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    this->setFocusPolicy(Qt::StrongFocus);
    this->verticalScrollBar()->setMinimum(0);
    this->verticalScrollBar()->setValue(0);
    this->verticalScrollBar()->setSingleStep(1);
    this->verticalScrollBar()->setPageStep(1);
    this->horizontalScrollBar()->setSingleStep(this->fontMetrics().boundingRect(" ").width());

    int maxwidth = qApp->primaryScreen()->size().width();
    this->horizontalScrollBar()->setMaximum(maxwidth);
    this->viewport()->setFixedWidth(maxwidth);

    m_blinktimer = new QTimer(this);
    m_blinktimer->setInterval(CURSOR_BLINK_INTERVAL);

    connect(m_blinktimer, &QTimer::timeout, this, &DisassemblerTextView::blinkCursor);

    connect(this, &DisassemblerTextView::customContextMenuRequested, [&](const QPoint&) {
        m_contextmenu->exec(QCursor::pos());
    });

    this->createContextMenu();
}

bool DisassemblerTextView::canGoBack() const { return m_disassembler->document()->cursor()->canGoBack(); }
bool DisassemblerTextView::canGoForward() const { return m_disassembler->document()->cursor()->canGoForward(); }

u64 DisassemblerTextView::visibleLines() const
{
    QFontMetrics fm = this->fontMetrics();
    u64 vl = std::ceil(this->height() / fm.height());

    if((vl <= 1) && (m_disassembler->document()->size() >= DOCUMENT_IDEAL_SIZE))
        return DOCUMENT_IDEAL_SIZE;

    return vl;
}

u64 DisassemblerTextView::firstVisibleLine() const { return this->verticalScrollBar()->value(); }
u64 DisassemblerTextView::lastVisibleLine() const { return this->firstVisibleLine() + this->visibleLines() - 1; }

void DisassemblerTextView::setDisassembler(REDasm::DisassemblerAPI *disassembler)
{
    m_disassembler = disassembler;

    REDasm::ListingDocument* doc = m_disassembler->document();
    REDasm::ListingCursor* cur = doc->cursor();

    m_disassembler->busyChanged += [&]() {
      if(m_disassembler->busy())
          return;

      m_disassembler->document()->moveToEP();
    };

    doc->changed += std::bind(&DisassemblerTextView::onDocumentChanged, this, std::placeholders::_1);
    cur->positionChanged += std::bind(&DisassemblerTextView::moveToSelection, this);
    cur->backChanged += [=]() { emit canGoBackChanged(); };
    cur->forwardChanged += [=]() { emit canGoForwardChanged(); };

    this->adjustScrollBars();
    connect(this->verticalScrollBar(), &QScrollBar::valueChanged, [&](int) { this->viewport()->update(); });

    m_renderer = std::make_unique<ListingTextRenderer>(this->font(), m_disassembler);
    m_disassemblerpopup = new DisassemblerPopup(m_disassembler, this);

    if(!m_disassembler->busy())
        cur->positionChanged();
}

void DisassemblerTextView::copy()
{
    if(!m_disassembler->document()->cursor()->hasSelection())
        return;

    qApp->clipboard()->setText(S_TO_QS(m_renderer->getSelectedText()));
}

void DisassemblerTextView::goTo(address_t address)
{
    REDasm::ListingDocument* doc = m_disassembler->document();
    auto it = doc->item(address);

    if(it == doc->end())
        return;

    this->goTo(it->get());
}

void DisassemblerTextView::goTo(REDasm::ListingItem *item)
{
    REDasm::ListingDocument* doc = m_disassembler->document();
    int idx = doc->indexOf(item);

    if(idx == -1)
        return;

    doc->cursor()->moveTo(idx);
}

void DisassemblerTextView::addComment()
{
    QString res = QInputDialog::getText(this, QString("Add Comment"), "Comment:", QLineEdit::Normal);
    m_disassembler->document()->comment(m_disassembler->document()->currentItem()->address, res.toStdString());
}
void DisassemblerTextView::goBack() { m_disassembler->document()->cursor()->goBack();  }
void DisassemblerTextView::goForward() { m_disassembler->document()->cursor()->goForward(); }

void DisassemblerTextView::blinkCursor()
{
    REDasm::ListingDocument* doc = m_disassembler->document();
    REDasm::ListingCursor* cur = doc->cursor();

    m_renderer->toggleCursor();

    if(!this->isLineVisible(cur->currentLine()))
        return;

    this->viewport()->update();
}

void DisassemblerTextView::scrollContentsBy(int dx, int dy)
{
    if(dx)
    {
        QWidget* viewport = this->viewport();
        viewport->move(viewport->x() + dx, viewport->y());
        return;
    }

    QAbstractScrollArea::scrollContentsBy(dx, dy);
}

void DisassemblerTextView::focusInEvent(QFocusEvent *e)
{
    m_renderer->enableCursor();
    m_blinktimer->start();

    QAbstractScrollArea::focusInEvent(e);
}

void DisassemblerTextView::focusOutEvent(QFocusEvent *e)
{
    m_blinktimer->stop();
    m_renderer->disableCursor();

    QAbstractScrollArea::focusOutEvent(e);
}

void DisassemblerTextView::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)

    if(!m_renderer)
        return;

    QPainter painter(this->viewport());
    painter.setFont(this->font());
    m_renderer->render(this->firstVisibleLine(), this->visibleLines(), &painter);
}

void DisassemblerTextView::resizeEvent(QResizeEvent *e)
{
    QAbstractScrollArea::resizeEvent(e);
    this->adjustScrollBars();
}

void DisassemblerTextView::mousePressEvent(QMouseEvent *e)
{
    REDasm::ListingCursor* cur = m_disassembler->document()->cursor();

    if((e->button() == Qt::LeftButton) || (!cur->hasSelection() && (e->button() == Qt::RightButton)))
    {
        e->accept();
        REDasm::ListingCursor::Position cp = m_renderer->hitTest(e->pos(), this->firstVisibleLine());
        cur->moveTo(cp.first, cp.second);
        m_renderer->highlightWordUnderCursor();
    }

    QAbstractScrollArea::mousePressEvent(e);
}

void DisassemblerTextView::mouseMoveEvent(QMouseEvent *e)
{
    if(e->buttons() == Qt::LeftButton)
    {
        e->accept();

        if(m_blinktimer->isActive())
        {
            m_blinktimer->stop();
            m_renderer->disableCursor();
        }

        REDasm::ListingCursor* cur = m_disassembler->document()->cursor();
        REDasm::ListingCursor::Position cp = m_renderer->hitTest(e->pos(), this->firstVisibleLine());
        cur->select(cp.first, cp.second);
        e->accept();
        return;
    }

    QAbstractScrollArea::mouseMoveEvent(e);
}

void DisassemblerTextView::mouseReleaseEvent(QMouseEvent *e)
{
    if(e->button() == Qt::LeftButton)
    {
        e->accept();

        if(!m_blinktimer->isActive())
            m_blinktimer->start();
    }

    QAbstractScrollArea::mouseReleaseEvent(e);
}

void DisassemblerTextView::mouseDoubleClickEvent(QMouseEvent *e)
{
    if(e->button() == Qt::LeftButton)
    {
        e->accept();

        if(this->followUnderCursor())
            return;

        REDasm::ListingCursor* cur = m_disassembler->document()->cursor();
        ListingTextRenderer::Range r = m_renderer->wordHitTest(e->pos(), this->firstVisibleLine());

        if(r.first == -1)
            return;

        cur->moveTo(cur->currentLine(), r.first);
        cur->select(cur->currentLine(), r.second);
        return;
    }

    QAbstractScrollArea::mouseReleaseEvent(e);
}

void DisassemblerTextView::keyPressEvent(QKeyEvent *e)
{
    REDasm::ListingDocument* doc = m_disassembler->document();
    REDasm::ListingCursor* cur = doc->cursor();

    m_blinktimer->stop();
    m_renderer->enableCursor();

    if(e->matches(QKeySequence::MoveToNextChar) || e->matches(QKeySequence::SelectNextChar))
    {
        u64 len = m_renderer->getLastColumn(cur->currentLine());

        if(e->matches(QKeySequence::MoveToNextChar))
            cur->moveTo(cur->currentLine(), std::min(len, cur->currentColumn() + 1));
        else
            cur->select(cur->currentLine(), std::min(len, cur->currentColumn() + 1));
    }
    else if(e->matches(QKeySequence::MoveToPreviousChar) || e->matches(QKeySequence::SelectPreviousChar))
    {
        if(e->matches(QKeySequence::MoveToPreviousChar))
            cur->moveTo(cur->currentLine(), std::max(static_cast<u64>(0), cur->currentColumn() - 1));
        else
            cur->select(cur->currentLine(), std::max(static_cast<u64>(0), cur->currentColumn() - 1));
    }
    else if(e->matches(QKeySequence::MoveToNextLine) || e->matches(QKeySequence::SelectNextLine))
    {
        if(doc->lastLine()  == cur->currentLine())
            return;

        int nextline = cur->currentLine() + 1;

        if(e->matches(QKeySequence::MoveToNextLine))
            cur->moveTo(nextline, std::min(cur->currentColumn(), m_renderer->getLastColumn(nextline)));
        else
            cur->select(nextline, std::min(cur->currentColumn(), m_renderer->getLastColumn(nextline)));
    }
    else if(e->matches(QKeySequence::MoveToPreviousLine) || e->matches(QKeySequence::SelectPreviousLine))
    {
        if(!cur->currentLine())
            return;

        int prevline = cur->currentLine() - 1;

        if(e->matches(QKeySequence::MoveToPreviousLine))
            cur->moveTo(prevline, std::min(cur->currentColumn(), m_renderer->getLastColumn(prevline)));
        else
            cur->select(prevline, std::min(cur->currentColumn(), m_renderer->getLastColumn(prevline)));
    }
    else if(e->matches(QKeySequence::MoveToNextPage) || e->matches(QKeySequence::SelectNextPage))
    {
        if(doc->lastLine()  == cur->currentLine())
            return;

        int pageline = std::min(doc->lastLine(), this->firstVisibleLine() + this->visibleLines());

        if(e->matches(QKeySequence::MoveToNextPage))
            cur->moveTo(pageline, std::min(cur->currentColumn(), m_renderer->getLastColumn(pageline)));
        else
            cur->select(pageline, std::min(cur->currentColumn(), m_renderer->getLastColumn(pageline)));
    }
    else if(e->matches(QKeySequence::MoveToPreviousPage) || e->matches(QKeySequence::SelectPreviousPage))
    {
        if(!cur->currentLine())
            return;

        u64 pageline = std::max(static_cast<u64>(0), this->firstVisibleLine() - this->visibleLines());

        if(e->matches(QKeySequence::MoveToPreviousPage))
            cur->moveTo(pageline, std::min(cur->currentColumn(), m_renderer->getLastColumn(pageline)));
        else
            cur->select(pageline, std::min(cur->currentColumn(), m_renderer->getLastColumn(pageline)));
    }
    else if(e->matches(QKeySequence::MoveToStartOfDocument) || e->matches(QKeySequence::SelectStartOfDocument))
    {
        if(!cur->currentLine())
            return;

        if(e->matches(QKeySequence::MoveToStartOfDocument))
            cur->moveTo(0, 0);
        else
            cur->select(0, 0);
    }
    else if(e->matches(QKeySequence::MoveToEndOfDocument) || e->matches(QKeySequence::SelectEndOfDocument))
    {
        if(doc->lastLine() == cur->currentLine())
            return;

        if(e->matches(QKeySequence::MoveToEndOfDocument))
            cur->moveTo(doc->lastLine(), m_renderer->getLastColumn(doc->lastLine()));
        else
            cur->select(doc->lastLine(), m_renderer->getLastColumn(doc->lastLine()));
    }
    else if(e->matches(QKeySequence::MoveToStartOfLine) || e->matches(QKeySequence::SelectStartOfLine))
    {
        if(e->matches(QKeySequence::MoveToStartOfLine))
            cur->moveTo(cur->currentLine(), 0);
        else
            cur->select(cur->currentLine(), 0);
    }
    else if(e->matches(QKeySequence::MoveToEndOfLine) || e->matches(QKeySequence::SelectEndOfLine))
    {
        if(e->matches(QKeySequence::MoveToEndOfLine))
            cur->moveTo(cur->currentLine(), m_renderer->getLastColumn(cur->currentLine()));
        else
            cur->select(cur->currentLine(), m_renderer->getLastColumn(cur->currentLine()));
    }
    else if(e->matches(QKeySequence::Copy))
        this->copy();
    else if(e->key() == Qt::Key_X)
        this->showReferencesUnderCursor();
    else if(e->key() == Qt::Key_N)
        this->renameCurrentSymbol();
    else if(e->key() == Qt::Key_Space)
        emit switchView();

    m_blinktimer->start();
}

bool DisassemblerTextView::event(QEvent *e)
{
    if(m_disassembler && !m_disassembler->busy() && (e->type() == QEvent::ToolTip))
    {
        QHelpEvent* helpevent = static_cast<QHelpEvent*>(e);
        this->showPopup(helpevent->pos());
        return true;
    }

    return QAbstractScrollArea::event(e);
}

void DisassemblerTextView::onDocumentChanged(const REDasm::ListingDocumentChanged *ldc)
{
    QScrollBar* vscrollbar = this->verticalScrollBar();
    this->adjustScrollBars();

    if(!this->isVisible() || (ldc->index < static_cast<u64>(vscrollbar->value())) || (ldc->index > vscrollbar->value() + this->visibleLines()))
        return;

    this->viewport()->update();
}

REDasm::SymbolPtr DisassemblerTextView::symbolUnderCursor()
{
    REDasm::ListingDocument* doc = m_disassembler->document();
    REDasm::ListingCursor* cur = doc->cursor();

    if(!cur->hasWordUnderCursor())
        return NULL;

    return doc->symbol(cur->wordUnderCursor());
}

bool DisassemblerTextView::isLineVisible(u64 line) const
{
    if(line < this->firstVisibleLine())
        return false;

    if(line > this->lastVisibleLine())
        return false;

    return true;
}

bool DisassemblerTextView::isColumnVisible(u64 column, u64 *xpos)
{
    QScrollBar* hscrollbar = this->horizontalScrollBar();
    u64 lastxpos = hscrollbar->value() + this->width();

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    u64 adv = this->fontMetrics().horizontalAdvance(" ");
#else
    u64 adv = this->fontMetrics().width(" ");
#endif

    *xpos = adv * column;

    if(*xpos > lastxpos)
    {
        *xpos -= this->width();
        return false;
    }
    else if(*xpos < static_cast<u64>(hscrollbar->value()))
    {
        *xpos = hscrollbar->value() - *xpos;
        return false;
    }

    return true;
}

void DisassemblerTextView::adjustScrollBars()
{
    if(!m_disassembler)
        return;

    QScrollBar* vscrollbar = this->verticalScrollBar();
    REDasm::ListingDocument* doc = m_disassembler->document();

    if(doc->size() <= static_cast<size_t>(this->visibleLines()))
        vscrollbar->setMaximum(static_cast<int>(doc->size()));
    else
        vscrollbar->setMaximum(static_cast<int>(doc->size() - this->visibleLines() + 1));
}

void DisassemblerTextView::moveToSelection()
{
    REDasm::ListingDocument* doc = m_disassembler->document();
    REDasm::ListingCursor* cur = doc->cursor();

    if(this->isLineVisible(cur->currentLine()))
    {
        this->viewport()->update();
        m_renderer->highlightWordUnderCursor();
    }
    else // Center on selection
    {
        QScrollBar* vscrollbar = this->verticalScrollBar();
        vscrollbar->setValue(std::max(static_cast<u64>(0), cur->currentLine() - this->visibleLines() / 2));
    }

    u64 xpos = 0;

    if(!this->isColumnVisible(cur->currentColumn(), &xpos))
    {
        QScrollBar* hscrollbar = this->horizontalScrollBar();
        hscrollbar->setValue(xpos);
    }

    REDasm::ListingItem* item = doc->itemAt(cur->currentLine());

    if(item)
        emit addressChanged(item->address);
}

void DisassemblerTextView::createContextMenu()
{
    m_contextmenu = new QMenu(this);
    m_actrename = m_contextmenu->addAction("Rename", [=]() { this->renameCurrentSymbol(); } );

    m_contextmenu->addSeparator();
    m_actxrefs = m_contextmenu->addAction("Cross References", [&]() { this->showReferencesUnderCursor(); });
    m_actfollow = m_contextmenu->addAction("Follow", [&]() { this->followUnderCursor(); });
    m_actgoto = m_contextmenu->addAction("Goto...", this, &DisassemblerTextView::gotoRequested);
    m_actcallgraph = m_contextmenu->addAction("Call Graph", [this]() { this->showCallGraph(); });
    m_acthexdump = m_contextmenu->addAction("Hex Dump", [this]() { });
    m_actcomment = m_contextmenu->addAction("Add Comment", this, &DisassemblerTextView::addComment);
    m_contextmenu->addSeparator();
    m_actback = m_contextmenu->addAction("Back", this, &DisassemblerTextView::goBack);
    m_actforward = m_contextmenu->addAction("Forward", this, &DisassemblerTextView::goForward);
    m_contextmenu->addSeparator();
    m_actcopy = m_contextmenu->addAction("Copy", this, &DisassemblerTextView::copy);

    connect(m_contextmenu, &QMenu::aboutToShow, this, &DisassemblerTextView::adjustContextMenu);
}

void DisassemblerTextView::adjustContextMenu()
{
    REDasm::ListingDocument* doc = m_disassembler->document();
    REDasm::SymbolPtr symbol = this->symbolUnderCursor();
    REDasm::ListingItem* item = doc->currentItem();
    REDasm::Segment* segment = NULL;

    m_actback->setVisible(this->canGoBack());
    m_actforward->setVisible(this->canGoForward());
    m_actcopy->setVisible(doc->cursor()->hasSelection());

    if(!symbol)
    {
        segment = doc->segment(item->address);
        symbol = doc->functionStartSymbol(item->address);

        m_actrename->setVisible(false);
        m_actxrefs->setVisible(false);
        m_actfollow->setVisible(false);

        if(symbol)
            m_actcallgraph->setText(QString("Callgraph %1").arg(S_TO_QS(symbol->name)));

        m_actcallgraph->setVisible(symbol && segment && segment->is(REDasm::SegmentTypes::Code));
        m_acthexdump->setVisible(false);
        return;
    }

    segment = doc->segment(symbol->address);

    m_actxrefs->setVisible(true);
    m_actxrefs->setText(QString("Cross Reference %1").arg(S_TO_QS(symbol->name)));

    m_actrename->setText(QString("Rename %1").arg(S_TO_QS(symbol->name)));
    m_actrename->setVisible(!symbol->isLocked());

    m_actcallgraph->setVisible(symbol->isFunction());
    m_actcallgraph->setText(QString("Callgraph %1").arg(S_TO_QS(symbol->name)));

    m_actfollow->setText(QString("Follow %1").arg(S_TO_QS(symbol->name)));
    m_actfollow->setVisible(symbol->is(REDasm::SymbolTypes::Code));

    m_actcomment->setVisible(item->is(REDasm::ListingItem::InstructionItem));

    m_acthexdump->setVisible(segment && !segment->is(REDasm::SegmentTypes::Bss));
    m_acthexdump->setVisible(segment && !segment->is(REDasm::SegmentTypes::Bss));
}

void DisassemblerTextView::showReferencesUnderCursor()
{
    REDasm::SymbolPtr symbol = this->symbolUnderCursor();

    if(!symbol)
        return;

    emit referencesRequested(symbol->address);
}

bool DisassemblerTextView::followUnderCursor()
{
    REDasm::SymbolPtr symbol = this->symbolUnderCursor();

    if(!symbol)
        return false;

    this->goTo(symbol->address);
    return true;
}

void DisassemblerTextView::showCallGraph()
{
    REDasm::SymbolPtr symbol = this->symbolUnderCursor();

    if(!symbol)
    {
        REDasm::ListingDocument* doc = m_disassembler->document();
        REDasm::ListingItem* item = doc->currentItem();
        symbol = doc->functionStartSymbol(item->address);
    }

    emit callGraphRequested(symbol->address);
}

void DisassemblerTextView::showPopup(const QPoint& pos)
{
    std::string word = m_renderer->getWordUnderCursor(pos, this->firstVisibleLine());

    if(!word.empty())
    {
        REDasm::ListingCursor::Position cp = m_renderer->hitTest(pos, this->firstVisibleLine());
        m_disassemblerpopup->popup(word, cp.first);
        return;
    }

    m_disassemblerpopup->hide();
}

void DisassemblerTextView::renameCurrentSymbol()
{
    REDasm::SymbolPtr symbol = this->symbolUnderCursor();

    if(!symbol || symbol->isLocked())
        return;

    REDasm::ListingDocument* doc = m_disassembler->document();

    QString symbolname = S_TO_QS(symbol->name);
    QString res = QInputDialog::getText(this, QString("Rename %1").arg(symbolname), "Symbol name:", QLineEdit::Normal, symbolname);

    if(doc->symbol(res.toStdString()))
    {
        QMessageBox::warning(this, "Rename failed", "Duplicate symbol name");
        this->renameCurrentSymbol();
        return;
    }

    doc->rename(symbol->address, res.toStdString());
}
