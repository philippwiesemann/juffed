/*
JuffEd - An advanced text editor
Copyright 2007-2010 Mikhail Murzin

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License 
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <QDebug>

#include "SciDoc.h"

#include "AutocompleteSettings.h"
#include "Functions.h"
#include "JuffScintilla.h"
#include "Log.h"
#include "EditorSettings.h"
#include "LexerStorage.h"
#include "MainSettings.h"
#include "QSciSettings.h"

#include <QFile>
#include <QTextCodec>
#include <QTextStream>
#include <QVBoxLayout>
#include <QPrintDialog>
#include <QScrollBar>
#include <QSplitter>

//#include <Qsci/qsciscintilla.h>
#include <Qsci/qsciprinter.h>
#include <Qsci/qscilexer.h>

class SciDoc::Interior {
public:
	Interior(QWidget* w) {
		LOGGER;
		
		curEdit_ = NULL;
		
		spl_ = new QSplitter(Qt::Vertical);
		QVBoxLayout* vBox = new QVBoxLayout();
		vBox->setContentsMargins(0, 0, 0, 0);
		vBox->addWidget(spl_);
		w->setLayout(vBox);
		
		edit1_ = createEdit();
		edit2_ = createEdit();
		spl_->addWidget(edit1_);
		spl_->addWidget(edit2_);
		edit1_->setDocument(edit2_->document());
		w->setFocusProxy(spl_);
		spl_->setSizes(QList<int>() << 0 << spl_->height());
	}
	
	JuffScintilla* createEdit() {
		LOGGER;
		JuffScintilla* edit = new JuffScintilla();
		edit->setUtf8(true);
		edit->setFolding(QsciScintilla::BoxedTreeFoldStyle);
		edit->setAutoIndent(true);
		edit->setBraceMatching(QsciScintilla::SloppyBraceMatch);
		
		edit->setMarginLineNumbers(1, true);
		edit->setMarginSensitivity(1, true);
		edit->setMarginWidth(2, 12);
		//	set the 1st margin accept markers 
		//	number 1 and 2 (binary mask 00000110 == 6)
		edit->setMarginMarkerMask(1, 6);
		edit->markerDefine(QsciScintilla::RightTriangle, 1);
		edit->markerDefine(QsciScintilla::Background, 2);
		edit->setMarkerForegroundColor(QColor(100, 100, 100));
//		edit->setMarkerBackgroundColor(TextDocSettings::markersColor());
//		edit->setMarginsBackgroundColor(QColor(50, 50, 50));
//		edit->setMarginsForegroundColor(QColor(150, 150, 150));
//		edit->setFoldMarginColors(QColor(150, 150, 150), QColor(50, 50, 50));

		return edit;
	}
	
	void setCurrentEdit(JuffScintilla* edit) {
		LOGGER;
		
//		if ( edit == edit1_ )
//			qDebug() << "edit1";
//		else if ( edit == edit2_ )
//			qDebug() << "edit2";
//		else
//			qDebug() << "edit ??";
		curEdit_ = edit;
		spl_->setFocusProxy(edit);
	}
	
	JuffScintilla* edit1_;
	JuffScintilla* edit2_;
	JuffScintilla* curEdit_;
	QString syntax_;
	QSplitter* spl_;
	QTimer* hlTimer_;
//	QWidget* parent_;
};

SciDoc::SciDoc(const QString& fileName) : Juff::Document(fileName) {
	LOGGER;
	
	int_ = new Interior(this);
	
	JuffScintilla* edits[] = { int_->edit1_, int_->edit2_ };
	for ( int i = 0; i < 2; ++i) {
		JuffScintilla* edit = edits[i];
		connect(edit, SIGNAL(modificationChanged(bool)), this, SIGNAL(modified(bool)));
		connect(edit, SIGNAL(cursorPositionChanged(int, int)), this, SLOT(onCursorMoved(int, int)));
	//	connect(int_->edit1_, SIGNAL(contextMenuCalled(int, int)), this, SIGNAL(contextMenuCalled(int, int)));
		connect(edit, SIGNAL(marginClicked(int, int, Qt::KeyboardModifiers)), SLOT(onMarginClicked(int, int, Qt::KeyboardModifiers)));
		connect(edit, SIGNAL(linesChanged()), SLOT(onLineCountChanged()));
		connect(edit, SIGNAL(focusReceived()), SLOT(onEditFocused()));
		connect(edit, SIGNAL(textChanged()), this, SIGNAL(textChanged()));
	}
	
	QString lexName = "none";
	if ( !fileName.isEmpty() && !Juff::isNoname(fileName) ) {
		QString codecName = Document::guessCharset(fileName);
		if ( !codecName.isEmpty() )
			setCharset(codecName);
		readFile();
		int_->edit1_->setModified(false);

		//	syntax highlighting
		lexName = LexerStorage::instance()->lexerName(fileName);
	}

	setLexer(lexName);
	
	applySettings();
	
	QAction* hlWordAct = new QAction("", this);
	hlWordAct->setShortcut(QKeySequence("Ctrl+H"));
	connect(hlWordAct, SIGNAL(triggered()), SLOT(highlightWord()));
	addAction(hlWordAct);
	
	startCheckingTimer();
	
	int_->hlTimer_ = new QTimer();
	connect(int_->hlTimer_, SIGNAL(timeout()), SLOT(highlightWord()));
	int_->hlTimer_->setSingleShot(true);
}

/*SciDoc::SciDoc(Juff::Document* doc) : Juff::Document(doc) {
	SciDoc* d = qobject_cast<SciDoc*>(doc);
	if ( d != 0 ) {
		int_->edit1_->setDocument(d->int_->edit1_->document());
		int_->edit2_->setDocument(d->int_->edit2_->document());
	}
}*/

SciDoc::~SciDoc() {
	delete int_;
}

QString SciDoc::type() const {
	return "QSci";
}

bool SciDoc::supportsAction(Juff::ActionID id) const {
	switch (id) {
		case Juff::FileClone : return true;
		default :              return Juff::Document::supportsAction(id);
	}
}

/*Juff::Document* SciDoc::createClone() {
	if ( hasClone() )
		return NULL;
	else
		return new SciDoc(this);
}

void SciDoc::updateClone() {
	LOGGER;
	
//	SciDoc* cln = qobject_cast<SciDoc*>(clone());
//	if ( cln != 0 ) {
//		if ( cln->int_->syntax_ != int_->syntax_ ) {
//			cln->int_->syntax_ = int_->syntax_;
//			QsciLexer* lexer = LexerStorage::instance()->lexer(int_->syntax_);
//			cln->int_->edit1_->setLexer(lexer);
//			cln->int_->edit2_->setLexer(lexer);
//		}
//	}
	
	Juff::Document::updateClone();
}*/

void SciDoc::init() {
	int_->setCurrentEdit(int_->edit2_);
}

void SciDoc::reload() {
	if ( !Juff::isNoname(this) ) {
		int line, col;
		getCursorPos(line, col);
		int scroll = scrollPos();
		readFile();
		setModified(false);
		if ( line >= 0 && col >= 0 ) {
			setCursorPos(line, col);
			setScrollPos(scroll);
		}
	}
}

void SciDoc::print() {
	QsciPrinter prn;
	QPrintDialog dlg(&prn, this);
	if (dlg.exec() == QDialog::Accepted) {
//		prn.setWrapMode(TextDocSettings::widthAdjust() || PrintSettings::alwaysWrap() ? QsciScintilla::WrapWord : QsciScintilla::WrapNone);
		
		int line1(-1), line2(-1), col1(-1), col2(-1);
		JuffScintilla* edit = int_->curEdit_;
		if ( edit ) {
//			QsciLexer* lexer = edit->lexer();
//			if ( !PrintSettings::keepBgColor() ) {
//				lexer->setDefaultPaper(Qt::white);
//				lexer->setPaper(Qt::white);
//				lexer->setDefaultColor(Qt::black);
//			}
//			if ( !PrintSettings::keepColors() ) {
//				lexer->setColor(Qt::black);
//			}
			edit->getSelection(&line1, &col1, &line2, &col2);
			if (line1 >=0 && line2 >= 0 && col1 >= 0 && col2 >= 0) {
				//	We have selection. Print it.
				
				--line2;
				prn.printRange(edit, line1, line2);
			}
			else {
				//	We don't have selection. Print the whole text.
				prn.printRange(edit, 0);
			}
			QFont font = EditorSettings::font();
			LexerStorage::instance()->updateLexers(font);
		}
	}
}



////////////////////////////////////////////////////////////////////////////////
// Document API implementation

int SciDoc::lineCount() const {
	return int_->edit1_->lines();
}

bool SciDoc::isModified() const {
	return int_->edit1_->isModified();
}

bool SciDoc::hasSelectedText() const {
	if ( int_->curEdit_ == NULL ) return false;
	
	return int_->curEdit_->hasSelectedText();
}

bool SciDoc::getSelection(int& line1, int& col1, int& line2, int& col2) const {
	if ( int_->curEdit_ == NULL ) return false;
	
	int_->curEdit_->getSelection(&line1, &col1, &line2, &col2);
	return true;
}

bool SciDoc::getSelectedText(QString& text) const {
	if ( int_->curEdit_ == NULL ) return false;
	
	text = int_->curEdit_->selectedText();
	return true;
}

bool SciDoc::getText(QString& text) const {
	if ( int_->curEdit_ == NULL ) return false;
	
	text = int_->curEdit_->text();
	return true;
}

QString SciDoc::textLine(int line) const {
	if ( int_->curEdit_ == NULL ) return QString();
	
	if ( line >=0 && line < lineCount() ) {
		return int_->curEdit_->text(line);
	}
	else {
		return QString();
	}
}

bool SciDoc::getCursorPos(int& line, int& col) const {
	if ( int_->curEdit_ == NULL ) return false;

	int_->curEdit_->getCursorPosition(&line, &col);
	return true;
}

QString SciDoc::syntax() const {
	return int_->syntax_;
}

void SciDoc::setModified(bool modified) {
	int_->edit1_->setModified(modified);
}

void SciDoc::setSelection(int line1, int col1, int line2, int col2) {
	if ( int_->curEdit_ == NULL ) return;
	
	int_->curEdit_->setSelection(line1, col1, line2, col2);
	int_->curEdit_->ensureCursorVisible();
}

void SciDoc::removeSelectedText() {
	if ( int_->curEdit_ == NULL ) return;
	
	int_->curEdit_->removeSelectedText();
}

void SciDoc::replaceSelectedText(const QString& text) {
	if ( int_->curEdit_ == NULL ) return;
	
	int_->curEdit_->beginUndoAction();
	removeSelectedText();
	insertText(text);
	int_->curEdit_->endUndoAction();
}

void SciDoc::insertText(const QString& text) {
	if ( int_->curEdit_ == NULL ) return;
	
	int row, col;
	getCursorPos(row, col);
	int newLines = text.count(QRegExp("\r\n|\n|\r"));
	int_->curEdit_->insert(text);
	if ( newLines == 0 )
		setCursorPos(row, col + text.length());
}

void SciDoc::setCursorPos(int line, int col) {
	if ( int_->curEdit_ == NULL ) return;
	
	int_->curEdit_->setCursorPosition(line, col);
}

void SciDoc::setSyntax(const QString& lexName) {
	LOGGER;

	if ( lexName.isEmpty() )
		return;
	
	QString oldSyntax = int_->syntax_;
	setLexer(lexName);
//	updateClone();
	
	// notify plugins
	emit syntaxChanged(oldSyntax);
}
// End of Document API implementation
////////////////////////////////////////////////////////////////////////////////

int SciDoc::scrollPos() const {
	if ( int_->curEdit_ == NULL ) return 0;
	return int_->curEdit_->verticalScrollBar()->value();
}

void SciDoc::setScrollPos(int pos) {
	if ( int_->curEdit_ == NULL ) return;
	int_->curEdit_->verticalScrollBar()->setValue(pos);
}



void SciDoc::undo() {
	LOGGER;
	if ( int_->curEdit_ != NULL )
		int_->curEdit_->undo();
}

void SciDoc::redo() {
	if ( int_->curEdit_ != NULL )
		int_->curEdit_->redo();
}

void SciDoc::cut() {
	if ( int_->curEdit_ != NULL )
		int_->curEdit_->cut();
}

void SciDoc::copy() {
	if ( int_->curEdit_ != NULL )
		int_->curEdit_->copy();
}

void SciDoc::paste() {
	if ( int_->curEdit_ != NULL )
		int_->curEdit_->paste();
}

void SciDoc::gotoLine(int line) {
	if ( int_->curEdit_ != NULL )
		int_->curEdit_->setCursorPosition(line, 0);
}

void SciDoc::setWrapWords(bool wrap) {
	LOGGER;
	
	if ( wrap ) {
		int_->edit1_->setWrapMode(QsciScintilla::WrapWord);
		int_->edit2_->setWrapMode(QsciScintilla::WrapWord);
	}
	else {
		int_->edit1_->setWrapMode(QsciScintilla::WrapNone);
		int_->edit2_->setWrapMode(QsciScintilla::WrapNone);
	}
}

void SciDoc::setShowLineNumbers(bool show) {
	LOGGER;
	
	int_->edit1_->showLineNumbers(show);
	int_->edit2_->showLineNumbers(show);
}

void SciDoc::setShowWhitespaces(bool show) {
	LOGGER;
	
	int_->edit1_->setWhitespaceVisibility(show ? QsciScintilla::WsVisible : QsciScintilla::WsInvisible);
	int_->edit2_->setWhitespaceVisibility(show ? QsciScintilla::WsVisible : QsciScintilla::WsInvisible);
	int_->edit1_->setWrapVisualFlags(show ? QsciScintilla::WrapFlagByBorder : QsciScintilla::WrapFlagNone);
	int_->edit2_->setWrapVisualFlags(show ? QsciScintilla::WrapFlagByBorder : QsciScintilla::WrapFlagNone);
	EditorSettings::set(EditorSettings::ShowWhitespaces, show);
}

void SciDoc::setShowLineEndings(bool show) {
	LOGGER;
	
	int_->edit1_->setEolVisibility(show);
	int_->edit2_->setEolVisibility(show);
}

bool SciDoc::wrapWords() const {
	return int_->edit1_->wrapMode() == QsciScintilla::WrapWord;
}

bool SciDoc::lineNumbersVisible() const {
	return int_->edit1_->lineNumbersVisible();
}

bool SciDoc::whitespacesVisible() const {
	return int_->edit1_->whitespaceVisibility() == QsciScintilla::WsVisible;
}

bool SciDoc::lineEndingsVisible() const {
	return int_->edit1_->eolVisibility();
}

void SciDoc::zoomIn() {
	LOGGER;
	
	int_->edit1_->zoomIn();
	int_->edit2_->zoomIn();
}

void SciDoc::zoomOut() {
	LOGGER;
	
	int_->edit1_->zoomOut();
	int_->edit2_->zoomOut();
}

void SciDoc::zoom100() {
	LOGGER;
	
	int_->edit1_->zoomTo(0);
	int_->edit2_->zoomTo(0);
}

void SciDoc::toUpperCase() {
	if ( int_->curEdit_ != NULL ) {
		int_->curEdit_->SendScintilla(QsciScintilla::SCI_UPPERCASE);
	}
}

void SciDoc::toLowerCase() {
	if ( int_->curEdit_ != NULL ) {
		int_->curEdit_->SendScintilla(QsciScintilla::SCI_LOWERCASE);
	}
}

void SciDoc::swapLines() {
	if ( int_->curEdit_ != NULL ) {
		int_->curEdit_->SendScintilla(QsciScintilla::SCI_LINETRANSPOSE);
	}
}

void SciDoc::moveUp() {
	if ( int_->curEdit_ == NULL ) return;
	
	if ( hasSelectedText() ) {
		int line1, line2, col1, col2;
		getSelection(line1, col1, line2, col2);
		
		if ( line1 == 0 )
			return;
		
		int realLine2 = line2;
		if ( col2 == 0 )
			--line2;
		
		int_->curEdit_->beginUndoAction();
		for (int line = line1; line <= line2; ++line) {
			int_->curEdit_->setCursorPosition(line, 0);
			swapLines();
		}
		
		setSelection(line1 - 1, col1, realLine2 - 1, col2);
		int_->curEdit_->endUndoAction();
	}
	else {
		int line, col;
		int_->curEdit_->getCursorPosition(&line, &col);
		if ( line > 0 ) {
			swapLines();
			int_->curEdit_->setCursorPosition(line - 1, col);
		}
	}
}

void SciDoc::moveDown() {
	if ( int_->curEdit_ == NULL ) return;
	
	if ( hasSelectedText() ) {
		int line1, line2, col1, col2;
		getSelection(line1, col1, line2, col2);
//		int n = lineCount();
		
		int realLine2 = line2;
		if ( col2 == 0 )
			--line2;
		
		if ( line2 == lineCount() - 1 )
			return;
		
		int_->curEdit_->beginUndoAction();
		for (int line = line2 + 1; line >= line1 + 1; --line) {
			int_->curEdit_->setCursorPosition(line, 0);
			swapLines();
		}
		
		setSelection(line1 + 1, col1, realLine2 + 1, col2);
		int_->curEdit_->endUndoAction();
	}
	else {
		int line, col;
		int_->curEdit_->getCursorPosition(&line, &col);
		if ( line < lineCount() - 1 ) {
			int_->curEdit_->setCursorPosition(line + 1, 0);
			swapLines();
			int_->curEdit_->setCursorPosition(line + 1, col);
		}
	}
}

void SciDoc::toggleCommentLines() {
	LOGGER;
	
	JuffScintilla* edit = int_->curEdit_;
	if ( edit == NULL ) return;

	QString comment;
	QString s = syntax();
	if ( s == "C++" || s == "PHP" || s == "C#" || s == "Java" || s == "JavaScript" )
		comment = "//";
	else if ( s == "Bash" || s == "Python" || s == "CMake" || s == "Makefile" )
		comment = "#";
	else if ( s == "Fortran" )
		comment = "!";
	//	TODO : need to add more syntaxes

	if ( comment.isEmpty() )
		return;

	if ( edit->hasSelectedText() ) {
		int line1, col1, line2, col2, curLine, curCol;
		edit->getSelection(&line1, &col1, &line2, &col2);
		edit->getCursorPosition(&curLine, &curCol);

		QString str1 = edit->text(line1);
		QString ln = str1.simplified();
		bool toComment = true;
		if ( ln.startsWith(comment) ) {
			toComment = false;
		}

		if ( col2 == 0 )
			--line2;

		edit->beginUndoAction();
		for ( int line = line1; line <= line2; ++line ) {
			str1 = edit->text(line);
			if ( toComment ) {
				if ( !str1.simplified().startsWith(comment) )
					commentLine(edit, line, str1, comment);
			}
			else {
				if ( str1.simplified().startsWith(comment) )
					uncommentLine(edit, line, str1, comment);
			}
		}
		edit->endUndoAction();
		if ( curCol > 0 )
			edit->setCursorPosition(curLine, curCol + comment.length() * (toComment ? 1 : -1) );
		else
			edit->setCursorPosition(curLine, curCol);
	}
	else {
		int line1, col1;
		edit->getCursorPosition(&line1, &col1);
		QString str1 = edit->text(line1);

		QString ln = str1.simplified();
		if ( ln.startsWith(comment) ) {
			uncommentLine(edit, line1, str1, comment);
			edit->setCursorPosition(line1, col1 - comment.length());
		}
		else {
			commentLine(edit, line1, str1, comment);
			edit->setCursorPosition(line1, col1 + comment.length());
		}
	}
}

void SciDoc::toggleCommentBlock() {
	LOGGER;
	
	JuffScintilla* edit = int_->curEdit_;
	if ( edit == NULL ) return;

	QString commBeg, commEnd;
	QString s = syntax();
	if ( s == "C++" || s == "Java" || s == "C#" || s == "PHP" || s == "CSS" || s == "JavaScript" ) {
		commBeg = "/*";
		commEnd = "*/";
	}
	else if ( s == "HTML" || s == "XML" ) {
		commBeg = "<!--";
		commEnd = "-->";
	}
	else if ( s == "Python" ) {
		commBeg = "'''";
		commEnd = "'''";
	}
	//	TODO : need to add more syntaxes

	if ( commBeg.isEmpty() || commEnd.isEmpty() )
		return;

	if ( edit->hasSelectedText() ) {
		int line1, col1, line2, col2, curLine, curCol;
		edit->getSelection(&line1, &col1, &line2, &col2);
		edit->getCursorPosition(&curLine, &curCol);
		
		QString str1 = edit->selectedText();
		bool toComment = true;
		if ( str1.startsWith(commBeg) && str1.endsWith(commEnd) )
			toComment = false;

		QString str2;
		if ( toComment )
			str2 = commBeg + str1 + commEnd;
		else {
			str2 = str1;
			str2.chop(commEnd.length());
			str2.remove(0, commBeg.length());
		}
		replaceSelectedText(str2);
		if ( line1 == line2 ) {
			if ( curCol == col1 )
				edit->setCursorPosition(curLine, curCol);
			else
				edit->setCursorPosition(curLine, curCol + (commBeg.length() + commEnd.length()) * (toComment ? 1 : -1));
		}
		else {
			if ( curLine == line2 && curCol == col2)
				edit->setCursorPosition(curLine, curCol + commEnd.length() * (toComment ? 1 : -1) );
			else
				edit->setCursorPosition(curLine, curCol);
		}
	}
}

void SciDoc::duplicateText() {
	if ( int_->curEdit_ == NULL ) return;
	
	if ( int_->curEdit_->hasSelectedText() )
		int_->curEdit_->SendScintilla(QsciScintilla::SCI_SELECTIONDUPLICATE);
	else
		int_->curEdit_->SendScintilla(QsciScintilla::SCI_LINEDUPLICATE);
}

void SciDoc::unindent() {
	LOGGER;
	
	JuffScintilla* edit = int_->curEdit_;
	if ( edit == NULL ) return;
	
	int line1(-1), line2(-1), col1(-1), col2(-1);
	if ( edit->hasSelectedText() ) {
		edit->getSelection(&line1, &col1, &line2, &col2);
		if ( col2 == 0 )
			--line2;
		
		if (line1 <= line2 && line1 >= 0) {
			edit->beginUndoAction();
			for (int l = line1; l <= line2; ++l) {
				edit->unindent(l);
			}
			edit->endUndoAction();
		}
	}
	else {
		edit->getCursorPosition(&line1, &col1);
		if ( line1 >= 0 ) {
			edit->unindent(line1);
		}
	}
}

void SciDoc::removeLine() {
	LOGGER;
	
	JuffScintilla* edit = int_->curEdit_;
	if ( edit == NULL ) return;
	
	int line1(-1), line2(-1), col1(-1), col2(-1);
	if ( edit->hasSelectedText() ) {
		edit->getSelection(&line1, &col1, &line2, &col2);
		if ( col2 == 0 )
			--line2;
		
		if (line1 <= line2 && line1 >= 0) {
			setCursorPos(line1, 0);
			edit->beginUndoAction();
			for (int l = line1; l <= line2; ++l) {
				edit->SendScintilla(QsciScintilla::SCI_LINEDELETE);
			}
			edit->endUndoAction();
		}
	}
	else {
		edit->getCursorPosition(&line1, &col1);
		if ( line1 >= 0 ) {
			edit->SendScintilla(QsciScintilla::SCI_LINEDELETE);
		}
	}
}

void SciDoc::highlightWord() {
	LOGGER;
	
	JuffScintilla* edit = int_->curEdit_;
	if ( edit == NULL ) return;
	
	QString word = edit->wordUnderCursor();
	edit->highlightText(word);
}




void SciDoc::readFile() {
	LOGGER;

	QString text;
	QFile file(fileName());
	if ( file.open(QIODevice::ReadOnly) ) {
		
//		if ( !keepCharset ) {
//			QString codecName = Document::guessCharset(fileName());
//			QTextCodec* c = QTextCodec::codecForName(codecName.toAscii());
//			if ( c ) {
//				setCodec(c);
//				setCharset(codecName);
//			}
//		}
		
		QTextStream ts(&file);
		ts.setCodec(codec());
		int_->edit1_->setText(ts.readAll());
	}
}

bool SciDoc::save(QString& error) {
	LOGGER;

	if ( Juff::isNoname(fileName()) ) {
		error = tr("This is a Noname file and shouldn't be saved directly");
		return false;
	}

	if ( MainSettings::get(MainSettings::StripTrailingSpaces) )
		stripTrailingSpaces();

	QFile file(fileName());
	stopCheckingTimer();
	if ( file.open(QIODevice::WriteOnly) ) {
		QString text("");
		text = int_->edit1_->text();
		file.write(codec()->fromUnicode(text));
		file.close();
//		Document::save(error);
		int_->edit1_->setModified(false);
		startCheckingTimer();
		return true;
	}
	else {
		error = tr("Can't open file for writing");
		startCheckingTimer();
		return false;
	}
}

bool SciDoc::saveAs(const QString& fileName, QString& error) {
	bool result = Document::saveAs(fileName, error);
	if ( result ) {
		QString lexName = LexerStorage::instance()->lexerName(this->fileName());
		setLexer(lexName);
	}
	return result;
}

void SciDoc::setLexer(const QString& lexName) {
	LOGGER;

	if ( lexName.isEmpty() )
		return;
	
	int_->syntax_ = lexName;
	QsciLexer* lexer = LexerStorage::instance()->lexer(lexName);
//	loadAutocompletionAPI(lexName, lexer);
	int_->edit1_->setLexer(lexer);
	int_->edit2_->setLexer(lexer);
}

//void SciDoc::showLineNumbers(bool show) {
//	int_->edit1_->showLineNumbers(show);
//	int_->edit2_->showLineNumbers(show);
//}




void SciDoc::applySettings() {
	LOGGER;
	
	setShowLineNumbers(EditorSettings::get(EditorSettings::ShowLineNumbers));
	
	QFont font = EditorSettings::font();
	LexerStorage::instance()->updateLexers(font);
	
	QsciScintilla* edits[] = { int_->edit1_, int_->edit2_, NULL };
	for (int i = 0; edits[i] != NULL; ++i ) {
		QsciScintilla* edit = edits[i];
		
		edit->setTabWidth(EditorSettings::get(EditorSettings::TabWidth));
		edit->setIndentationsUseTabs(EditorSettings::get(EditorSettings::UseTabs));
		
		edit->setIndentationGuides(QSciSettings::get(QSciSettings::ShowIndents));
		edit->setIndentationGuidesForegroundColor(QSciSettings::get(QSciSettings::IndentsColor));
		edit->setIndentationGuidesBackgroundColor(EditorSettings::get(EditorSettings::DefaultBgColor));
		
		QColor selBgColor = EditorSettings::get(EditorSettings::SelectionBgColor);
		edit->setSelectionBackgroundColor(selBgColor);
		if ( selBgColor.red() + selBgColor.green() + selBgColor.blue() < 3 * 255 / 2)
			edit->setSelectionForegroundColor(QColor(255, 255, 255));
		else
			edit->setSelectionForegroundColor(QColor(0, 0, 0));
		
		if ( QSciSettings::get(QSciSettings::HighlightMatchingBrace) ) {
			edit->setMatchedBraceBackgroundColor(QSciSettings::get(QSciSettings::MatchingBraceBgColor));
			edit->setMatchedBraceForegroundColor(QSciSettings::get(QSciSettings::MatchingBraceFgColor));
		}
		else {
			edit->setMatchedBraceBackgroundColor(EditorSettings::get(EditorSettings::DefaultBgColor));
			edit->setMatchedBraceForegroundColor(EditorSettings::get(EditorSettings::DefaultFontColor));
		}
		
		
		int lInd = EditorSettings::get(EditorSettings::LineLengthIndicator);
		if ( lInd > 0 ) {
			edit->setEdgeMode(QsciScintilla::EdgeLine);
			edit->setEdgeColumn(lInd);
		}
		else {
			edit->setEdgeMode(QsciScintilla::EdgeNone);
		}
		
		edit->setCaretLineVisible(QSciSettings::get(QSciSettings::HighlightCurLine));
//		edit->setCaretLineVisible(TextDocSettings::highlightCurrentLine());
		edit->setCaretLineBackgroundColor(LexerStorage::instance()->curLineColor(syntax()));
//		edit->setIndentationGuides(TextDocSettings::showIndents());
//		edit->setBackspaceUnindents(TextDocSettings::backspaceUnindents());
//		edit->setMarkerBackgroundColor(TextDocSettings::markersColor());
		if ( QsciLexer* lexer = edit->lexer() ) {
//			lexer->setFont(font, -1);
//			edit->setCaretForegroundColor(lexer->defaultColor());
//			edit->setIndentationGuidesForegroundColor(TextDocSettings::indentsColor());
//			edit->setIndentationGuidesBackgroundColor(lexer->defaultPaper());

		}
//		edit->setMatchedBraceBackgroundColor(TextDocSettings::matchedBraceBgColor());
		
		// selection
/*		QColor selBgColor = TextDocSettings::selectionBgColor();
		edit->setSelectionBackgroundColor(selBgColor);
		if ( selBgColor.red() + selBgColor.green() + selBgColor.blue() < 3 * 255 / 2)
			edit->setSelectionForegroundColor(QColor(255, 255, 255));
		else
			edit->setSelectionForegroundColor(QColor(0, 0, 0));
*/
		//	autocompletion
		edit->setAutoCompletionThreshold(AutocompleteSettings::get(AutocompleteSettings::Threshold));
		edit->setAutoCompletionReplaceWord(AutocompleteSettings::get(AutocompleteSettings::ReplaceWord));
		edit->setAutoCompletionCaseSensitivity(AutocompleteSettings::get(AutocompleteSettings::CaseSensitive));
		if ( AutocompleteSettings::get(AutocompleteSettings::UseDocument) ) {
			if ( AutocompleteSettings::get(AutocompleteSettings::UseApis) )
				edit->setAutoCompletionSource(QsciScintilla::AcsAll);
			else
				edit->setAutoCompletionSource(QsciScintilla::AcsDocument);
		}
		else {
			if ( AutocompleteSettings::get(AutocompleteSettings::UseApis) )
				edit->setAutoCompletionSource(QsciScintilla::AcsAPIs);
			else
				edit->setAutoCompletionSource(QsciScintilla::AcsNone);
		}
	}
}



////////////////////////////////////////////////////////////////////////////////
// SLOTS

void SciDoc::onCursorMoved(int line, int col) {
	if ( int_->hlTimer_->isActive() )
		int_->hlTimer_->stop();
	if ( QSciSettings::get(QSciSettings::HighlightCurWord) )
		int_->hlTimer_->start(500);
	emit cursorPosChanged(line, col);
}

void SciDoc::onMarginClicked(int, int, Qt::KeyboardModifiers) {
}

void SciDoc::onLineCountChanged() {
	emit lineCountChanged(lineCount());
}

void SciDoc::onEditFocused() {
	LOGGER;

	if ( sender() == int_->edit1_ ) {
		int_->setCurrentEdit(int_->edit1_);
	}
	else {
		int_->setCurrentEdit(int_->edit2_);
	}
	emit focused();
}


////////////////////////////////////////////////////////////////////////////////
// Helper functions
void SciDoc::commentLine(JuffScintilla* edit, int line, const QString& str1, const QString& comment) {
	QString str2 = comment + str1;
	edit->setSelection(line, 0, line + 1, 0);
	replaceSelectedText(str2);
}

void SciDoc::uncommentLine(JuffScintilla* edit, int line, const QString& str1, const QString& comment) {
	int pos = str1.indexOf(comment);
	QString str2 = str1;
	str2.replace(pos, comment.length(), "");
	edit->setSelection(line, 0, line + 1, 0);
	replaceSelectedText(str2);
}

void SciDoc::stripTrailingSpaces() {
	LOGGER;
	if ( int_->curEdit_ == NULL ) return;

	int line, col;
	getCursorPos(line, col);
	QString text = int_->curEdit_->text();
	QStringList lines = text.split(QRegExp("\r\n|\r|\n"));
	QRegExp rx("[ \t]+$");
	int i = 0;
	foreach (QString str, lines) {
		int pos = str.indexOf(rx);
		if ( pos >= 0 ) {
			int_->curEdit_->setSelection(i, 0, i, str.length());
			str.truncate(pos);
			replaceSelectedText(str);
		}
		++i;
	}
	setCursorPos(line, col);
}


bool SciDoc::find(const Juff::SearchParams& params) {
//	if ( int_->curEdit_ == NULL) return false;
		
//	int_->curEdit_->find(params.findWhat);
}