#include <QDebug>

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

#include "SearchEngine.h"

#include "Document.h"
#include "JuffMW.h"
#include "Log.h"
#include "SearchPopup.h"
#include "SearchResults.h"

int findInString(const QString& line, const Juff::SearchParams& params, int& length);
QString selectedTextForSearch(Juff::Document* doc);


SearchEngine::SearchEngine(Juff::DocHandlerInt* handler, JuffMW* mw) : QObject() {
	mw_ = mw;
	handler_ = handler;
	searchPopup_ = mw_->searchPopup();
	curDoc_ = NULL;
	
//	connect(searchPopup_, SIGNAL(searchRequested()), SLOT(onSearchRequested()));
	connect(searchPopup_, SIGNAL(searchParamsChanged(const Juff::SearchParams&)), SLOT(onSearchParamsChanged(const Juff::SearchParams&)));
	connect(searchPopup_, SIGNAL(findNextRequested()), SLOT(onFindNext()));
	connect(searchPopup_, SIGNAL(findPrevRequested()), SLOT(onFindPrev()));
	connect(searchPopup_, SIGNAL(replaceNextRequested()), SLOT(onReplaceNext()));
	connect(searchPopup_, SIGNAL(replacePrevRequested()), SLOT(onReplacePrev()));
	connect(searchPopup_, SIGNAL(replaceAllRequested()), SLOT(onReplaceAll()));
	connect(searchPopup_, SIGNAL(closed()), SLOT(onDlgClosed()));
}

SearchEngine::~SearchEngine() {
}

void SearchEngine::setCurDoc(Juff::Document* doc) {
	LOGGER;
	
	changeCurDoc(doc);
	
//	if ( searchPopup_->isVisible() )
//		find();
}

void SearchEngine::find() {
	LOGGER;
	
	if ( curDoc_ == NULL || curDoc_->isNull() ) {
		qDebug("No document specified for SearchEngine");
		return;
	}
	
//	searchPopup_->hideReplace();
	searchPopup_->expand(false);
	searchPopup_->show();
	Juff::SearchResults* res = curDoc_->searchResults();
	if ( res != NULL ) {
		searchPopup_->setFindText(res->params().findWhat);
		curDoc_->highlightSearchResults();
//		res->setVisible(true);
	}
	else {
		searchPopup_->setFindText("");
	}
	searchPopup_->setFocusOnFind(true);
}

void SearchEngine::findNext() {
	LOGGER;
	
	if ( curDoc_ == NULL || curDoc_->isNull() ) {
		qDebug("No document specified for SearchEngine");
		return;
	}
	
	if ( searchPopup_->isVisible() ) {
		onFindNext();
	}
	else {
		Juff::SearchResults* res = curDoc_->searchResults();
		if ( res == NULL ) {
			find();
		}
		else {
			if ( res->count() > 0 ) {
				selectNextOccurence();
				curDoc_->setFocus();
			}
			else {
				// TODO :
			}
		}
	}
}

void SearchEngine::findPrev() {
	LOGGER;
	
	if ( curDoc_ == NULL || curDoc_->isNull() ) {
		qDebug("No document specified for SearchEngine");
		return;
	}
	
	if ( searchPopup_->isVisible() ) {
		onFindPrev();
	}
	else {
		Juff::SearchResults* res = curDoc_->searchResults();
		if ( res == NULL ) {
			find();
		}
		else {
			if ( res->count() > 0 ) {
				selectPrevOccurence();
				curDoc_->setFocus();
			}
			else {
				// TODO :
			}
		}
	}
}

void SearchEngine::replace() {
	LOGGER;
	
	if ( curDoc_ == NULL || curDoc_->isNull() ) {
		qDebug("No document specified for SearchEngine");
		return;
	}
	
	searchPopup_->setFindText(selectedTextForSearch(curDoc_));
	
	searchPopup_->expand(true);
	searchPopup_->show();
	searchPopup_->setFocusOnFind(true);
}

void SearchEngine::changeCurDoc(Juff::Document* doc) {
	if ( NULL != curDoc_ ) {
		curDoc_->disconnect(SIGNAL(textChanged()), this, SLOT(onDocTextChanged()));
	}
	curDoc_ = doc;
	connect(curDoc_, SIGNAL(textChanged()), this, SLOT(onDocTextChanged()));
	
	searchPopup_->hide();
	onDlgClosed();
	
//	if ( doc->searchResults() != NULL )
//		searchPopup_->setVisible(doc->searchResults()->isVisible());
//	else
//		searchPopup_->hide();
}

/*void SearchEngine::storePosition() {
	LOGGER;
	if ( curDoc_ == NULL ) return;
		
	curDoc_->getCursorPos(row_, col_);
}

void SearchEngine::restorePosition() {
	LOGGER;
	if ( curDoc_ == NULL ) return;
		
	curDoc_->setCursorPos(row_, col_);
}*/



void SearchEngine::onSearchParamsChanged(const Juff::SearchParams& params) {
	LOGGER;
	clearSelection();
	curDoc_->clearHighlighting();
	
//	const Juff::SearchParams& params = searchPopup_->searchParams();
	if ( params.findWhat.isEmpty() ) {
		searchPopup_->setSearchStatus(-1, 0);
		searchPopup_->setFocusOnFind();
		return;
	}
	
	Juff::SearchResults* results = performSearch(params);
	curDoc_->setSearchResults(results);
	
	if ( results == NULL ) {
		qDebug("Error: SearchResults can't be NULL, probably it's an error in getSearchResults() method");
		return;
	}
	
	if ( results->count() > 0 ) {
		selectNextOccurence();
		if ( searchPopup_->isVisible() ) {
			curDoc_->highlightSearchResults();
//			results->setVisible(true);
		}
	}
	else {
		searchPopup_->setSearchStatus(-1, 0);
	}
	
	searchPopup_->setFocusOnFind();
}

Juff::SearchResults* SearchEngine::performSearch(const Juff::SearchParams& params) {
	if ( curDoc_ == NULL || curDoc_->isNull() ) return NULL;
	
	QString text;
	curDoc_->getText(text);
	
	if ( text.isEmpty() )
		return NULL;
	
	if ( params.findWhat.isEmpty() )
		return NULL;
	
	Juff::SearchResults* results = NULL;
	/*if ( params.mode == Juff::SearchParams::RegExp ) {
		// TODO : 
	}
	else*/ if ( params.mode == Juff::SearchParams::MultiLineRegExp ) {
		// TODO : 
	}
	else {
		results = new Juff::SearchResults(params);
		
		QStringList lines = text.split(QRegExp("\r\n|\n|\r"));
		QStringList::iterator it = lines.begin();
		int lineIndex = 0;
		while ( it != lines.end() ) {
			QString lineStr = *it;
			int length;
			int pos = findInString(lineStr, params, length);
			int indent = 0;
			while ( pos >= 0 ) {
				results->addOccurence(lineIndex, indent + pos, lineIndex, indent + pos + length);
				indent += pos + length;
				lineStr = lineStr.remove(0, pos + length);
				pos = findInString(lineStr, params, length);
			}
			
			lineIndex++;
			it++;
		}
	}
	
	return results;
}

/**
* It DOESN'T check for results validity - check prior to calling this function.
*/
int SearchEngine::selectNextOccurence() {
	// set the cursor to the end of selection
	int line1, col1, line2, col2;
	if ( curDoc_->hasSelectedText() ) {
		curDoc_->getSelection(line1, col1, line2, col2);
	}
	else {
		curDoc_->getCursorPos(line2, col2);
	}
	
	Juff::SearchResults* results = curDoc_->searchResults();
	
	// find the next occurence
	int index = results->findIndexByCursorPos(line2, col2, true);
	
	if ( index >= 0 ) {
		const Juff::SearchOccurence& occ = results->occurence(index);
		// select it
		curDoc_->setSelection(occ.startRow, occ.startCol, occ.endRow, occ.endCol);
		// update search status
		searchPopup_->setSearchStatus(index, results->count());
	}
	return index;
}

/**
* It DOESN'T check for results validity - check prior to calling this function.
*/
int SearchEngine::selectPrevOccurence() {
	int line1, col1, line2, col2;
	if ( curDoc_->hasSelectedText() ) {
		curDoc_->getSelection(line1, col1, line2, col2);
	}
	else {
		curDoc_->getCursorPos(line1, col1);
	}
	
	Juff::SearchResults* results = curDoc_->searchResults();
	
	// find the previous occurence
	int index = results->findIndexByCursorPos(line1, col1, false);
	if ( index >= 0 ) {
		const Juff::SearchOccurence& occ = results->occurence(index);
		// select it
		curDoc_->setSelection(occ.startRow, occ.startCol, occ.endRow, occ.endCol);
		// update search status
		searchPopup_->setSearchStatus(index, results->count());
	}
	return index;
}

void SearchEngine::onFindNext() {
	LOGGER;
	
	if ( curDoc_ == NULL || curDoc_->isNull() ) return;
	
	Juff::SearchResults* results = curDoc_->searchResults();
	
	if ( results != NULL ) {
		if ( results->count() == 0 ) {
			searchPopup_->setSearchStatus(-1, 0);
			return;
		}
		
		curDoc_->highlightSearchResults();
		selectNextOccurence();
	}
	searchPopup_->setFocusOnFind();
}

void SearchEngine::onFindPrev() {
	if ( curDoc_ == NULL || curDoc_->isNull() ) return;
	
	Juff::SearchResults* results = curDoc_->searchResults();
	
	if ( results != NULL ) {
		if ( results->count() == 0 ) {
			searchPopup_->setSearchStatus(-1, 0);
			return;
		}
		
		curDoc_->highlightSearchResults();
		selectPrevOccurence();
	}
	searchPopup_->setFocusOnFind();
}

void SearchEngine::onReplaceNext() {
	LOGGER;
	
	if ( curDoc_ == NULL || curDoc_->isNull() ) return;
	const Juff::SearchParams& params = searchPopup_->searchParams();
	
	if ( curDoc_->hasSelectedText() ) {
		curDoc_->replaceSelectedText(params.replaceWith, true);
	}
	onFindNext();
	searchPopup_->setFocusOnReplace(false);
}

void SearchEngine::onReplacePrev() {
	LOGGER;
	
	if ( curDoc_ == NULL || curDoc_->isNull() ) return;
	const Juff::SearchParams& params = searchPopup_->searchParams();
	
	if ( curDoc_->hasSelectedText() ) {
		curDoc_->replaceSelectedText(params.replaceWith, false);
	}
	onFindPrev();
	searchPopup_->setFocusOnReplace(false);
}

void SearchEngine::onReplaceAll() {
	LOGGER;
	
	if ( curDoc_ == NULL || curDoc_->isNull() ) return;
	
	Juff::SearchResults* results = curDoc_->searchResults();
	if ( results == NULL ) return;
	
	const Juff::SearchParams& params = searchPopup_->searchParams();
	
//	storePosition();
	disconnect(curDoc_, SIGNAL(textChanged()), this, SLOT(onDocTextChanged()));
	
	curDoc_->setCursorPos(0, 0);
	int replacesMade = 0;
	int count = results->count();
	for ( int i = count - 1; i >= 0; --i ) {
		const Juff::SearchOccurence& occ = results->occurence(i);
		curDoc_->setSelection(occ.startRow, occ.startCol, occ.endRow, occ.endCol);
		curDoc_->replaceSelectedText(params.replaceWith);
		replacesMade++;
	}
	searchPopup_->setFocusOnReplace();
	
//	restorePosition();
	
	mw_->message(QIcon(), tr("Replace"), tr("Replacement finished (%1 replacements were made)").arg(replacesMade), 5);
	
	connect(curDoc_, SIGNAL(textChanged()), this, SLOT(onDocTextChanged()));
	
	curDoc_->setSearchResults(NULL);
}

void SearchEngine::onDlgClosed() {
	LOGGER;
	curDoc_->clearHighlighting();
//	if ( curDoc_->searchResults() != NULL )
//		curDoc_->searchResults()->setVisible(false);
	curDoc_->setFocus();
}

void SearchEngine::onDocTextChanged() {
	LOGGER;
	
	clearSelection();
	curDoc_->clearHighlighting();
	
	Juff::SearchParams params = searchPopup_->searchParams();
	params.backwards = false;
	Juff::SearchResults* results = performSearch(params);
	curDoc_->setSearchResults(results);
	if ( searchPopup_->isVisible() ) {
		curDoc_->highlightSearchResults();
		searchPopup_->setSearchStatus(-1, results->count());
	}
}



void SearchEngine::clearSelection() {
	LOGGER;
	if ( curDoc_ == NULL || curDoc_->isNull() || !curDoc_->hasSelectedText() ) return;
	
	int line1, col1, line2, col2;
	curDoc_->getSelection(line1, col1, line2, col2);
	if ( !searchPopup_->searchParams().backwards ) {
		curDoc_->setCursorPos(line1, col1);
	}
	else {
		curDoc_->setCursorPos(line2, col2);
	}
}



////////////////////////////////////////////////////////////
// Local helpers

QString selectedTextForSearch(Juff::Document* doc) {
	QString text;
	if ( doc->hasSelectedText() ) {
		int line1, line2, col1, col2;
		doc->getSelection(line1, col1, line2, col2);
		if ( line1 == line2 )
			doc->getSelectedText(text);
	}
	return text;
}

int findInString(const QString& line, const Juff::SearchParams& params, int& length) {
	QString str = params.findWhat;
	bool forward = !params.backwards;
	QRegExp regExp;
	if ( params.mode == Juff::SearchParams::WholeWords ) {
		regExp = QRegExp(QString("\\b%1\\b").arg(QRegExp::escape(str)));
	}
	else
		regExp = QRegExp(str);
	regExp.setCaseSensitivity(params.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);

	int index = -1;
	if ( params.mode != Juff::SearchParams::PlainText ) {
		index = ( forward ? line.indexOf(regExp) : line.lastIndexOf(regExp) );
		length = regExp.matchedLength();
		return index;
	}
	else {
		if ( !params.caseSensitive ) {
			index = ( forward ? line.indexOf(str, 0, Qt::CaseInsensitive) : line.lastIndexOf(str, -1, Qt::CaseInsensitive) );
		}
		else {
			index = ( forward ? line.indexOf(str) : line.lastIndexOf(str) );
		}
		length = str.length();
		return index;
	}
}
