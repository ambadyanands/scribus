/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/

#include <QSignalBlocker>

#include "propertywidget_pareffect.h"

#include "appmodehelper.h"
#include "appmodes.h"
#include "iconmanager.h"
#include "pageitem.h"
#include "pageitem_table.h"
#include "scribus.h"
#include "scribusdoc.h"
#include "selection.h"
#include "ui/sctreewidget.h"
#include "units.h"
#include "util.h"

PropertyWidget_ParEffect::PropertyWidget_ParEffect(QWidget *parent) : QFrame(parent), m_enhanced(NULL), m_item(NULL), m_ScMW(NULL)
{
	setupUi(this);
	setFrameStyle(QFrame::Box | QFrame::Plain);
	setLineWidth(1);
	layout()->setAlignment( Qt::AlignTop );

	languageChange();
	dropCapLines->setValue(2);

	if (m_doc)
		peCharStyleCombo->updateFormatList();
	fillBulletStrEditCombo();
	fillNumFormatCombo();
	enableParEffect(false);
	bulletCharTableButton->setIcon(IconManager::instance()->loadIcon("22/insert-table.png"));
	numStart->setMinimum(1);
	numStart->setMaximum(9999);
	numLevelSpin->setMinimum(1);
	numLevelSpin->setMaximum(3);
	dropCapLines->setMinimum(2);
	dropCapLines->setMaximum(99);
}

void PropertyWidget_ParEffect::setMainWindow(ScribusMainWindow* mw)
{
	m_ScMW = mw;

	connect(m_ScMW->appModeHelper, SIGNAL(AppModeChanged(int, int)), this, SLOT(handleAppModeChanged(int, int)));
	connect(m_ScMW, SIGNAL(UpdateRequest(int)), this  , SLOT(handleUpdateRequest(int)));
}

void PropertyWidget_ParEffect::setDoc(ScribusDoc *doc)
{
	if(doc == (ScribusDoc*) m_doc)
		return;

	if (m_doc)
	{
		disconnect(m_doc->m_Selection, SIGNAL(selectionChanged()), this, SLOT(handleSelectionChanged()));
		disconnect(m_doc             , SIGNAL(docChanged())      , this, SLOT(handleSelectionChanged()));
	}

	m_doc = doc;
	peCharStyleCombo->setDoc(doc);

	if (m_doc.isNull())
	{
		disconnectSignals();
		return;
	}

	m_unitRatio   = m_doc->unitRatio();
	m_unitIndex   = m_doc->unitIndex();

	fillNumerationsCombo();

	connect(m_doc->m_Selection, SIGNAL(selectionChanged()), this, SLOT(handleSelectionChanged()));
	connect(m_doc             , SIGNAL(docChanged())      , this, SLOT(handleSelectionChanged()));

	// Handle properties update when switching document
	handleSelectionChanged();
}

void PropertyWidget_ParEffect::setCurrentItem(PageItem *item)
{
	if (item && m_doc.isNull())
		setDoc(item->doc());

	m_item = item;
	disconnectSignals();

	if (!m_item) return;

	if (m_item->asTextFrame() || m_item->asPathText() || m_item->asTable())
	{
		configureWidgets();
		ParagraphStyle parStyle =  m_item->itemText.defaultStyle();
		if (m_doc->appMode == modeEdit || m_doc->appMode == modeEditTable)
			m_item->currentTextProps(parStyle);
		updateStyle(parStyle);
		connectSignals();
	}
}

void PropertyWidget_ParEffect::unitChange()
{
	if (!m_doc)
		return;

	m_unitRatio = m_doc->unitRatio();
	m_unitIndex = m_doc->unitIndex();

	bool sigBlocked = peOffset->blockSignals(true);
	peOffset->setNewUnit( m_unitIndex );
	peOffset->blockSignals(sigBlocked);
}

void PropertyWidget_ParEffect::fillNumerationsCombo()
{
	QStringList numNames;
	if (m_doc)
	{
		foreach (QString numName, m_doc->numerations.keys())
			numNames.append(numName);
		numNames.sort();
	}
	numNames.prepend("<local block>");
	numComboBox->clear();
	numComboBox->insertItems(0, numNames);
}

void PropertyWidget_ParEffect::updateCharStyles()
{
	peCharStyleCombo->updateFormatList();
}

void PropertyWidget_ParEffect::showCharStyle(const QString& name)
{
	bool blocked = peCharStyleCombo->blockSignals(true);
	peCharStyleCombo->setFormat(name);
	peCharStyleCombo->blockSignals(blocked);
}

void PropertyWidget_ParEffect::enableDropCap(bool enable)
{
//	dropCapRadio_->setChecked(enable);
	dropCapLines->setEnabled(enable);
	if (enable)
	{
		dropCapsGroup->show();
		enableBullet(false);
		enableNum(false);
	}
	else
		dropCapsGroup->hide();
}
void PropertyWidget_ParEffect::enableBullet(bool enable)
{
	bulletStrEdit->setVisible(enable);
	bulletCharTableButton->setVisible(enable);
	bullGroup->setVisible(enable);
	if (enable)
	{
		enableDropCap(false);
		enableNum(false);
	}
}
void PropertyWidget_ParEffect::enableNum(bool enable)
{
	numComboBox->setVisible(enable);
	numLevelSpin->setVisible(enable);
	numStart->setVisible(enable);
	numPrefix->setVisible(enable);
	numSuffix->setVisible(enable);
	numFormatCombo->setVisible(enable);
	numGroup->setVisible(enable);
	if (enable)
	{
		enableBullet(false);
		enableDropCap(false);
	}
}
void PropertyWidget_ParEffect::enableParEffect(bool enable)
{
	peOffset->setVisible(enable);
	peCharStyleCombo->setVisible(enable);
	peIndent->setVisible(enable);
	peGroup->setVisible(enable);
	if (!enable)
	{
		enableBullet(false);
		enableDropCap(false);
		enableNum(false);
		peCombo->setCurrentIndex(0);
	}
}

void PropertyWidget_ParEffect::insertMarks(PageItem *item)
{
	for (int index = 0; index < item->itemText.length(); ++index)
	{
		Mark* mark = item->itemText.mark(index);
		if ((mark != NULL) && (item->itemText.hasMark(index)))
		{
			mark->OwnPage = item->OwnPage;
			//itemPtr and itemName set to this frame only if mark type is different than MARK2ItemType
			if (!mark->isType(MARK2ItemType))
			{
				mark->setItemPtr(item);
				mark->setItemName(item->itemName());
			}

			//anchors and indexes has no visible inserts in text
			if (mark->isType(MARKAnchorType) || mark->isType(MARKIndexType))
				continue;

			//set note marker charstyle
			if (mark->isNoteType())
			{
				TextNote* note = mark->getNotePtr();
				if (note == NULL)
					continue;
				mark->setItemPtr(item);
				NotesStyle* nStyle = note->notesStyle();
				Q_ASSERT(nStyle != NULL);
				QString chsName = nStyle->marksChStyle();
				CharStyle currStyle(item->itemText.charStyle(index));
				if (!chsName.isEmpty())
				{
					CharStyle marksStyle(item->doc()->charStyle(chsName));
					if (!currStyle.equiv(marksStyle))
					{
						currStyle.setParent(chsName);
						item->itemText.applyCharStyle(index, 1, currStyle);
					}
				}

				StyleFlag s(item->itemText.charStyle(index).effects());
				if (mark->isType(MARKNoteMasterType))
				{
					if (nStyle->isSuperscriptInMaster())
						s |= ScStyle_Superscript;
					else
						s &= ~ScStyle_Superscript;
				}
				else
				{
					if (nStyle->isSuperscriptInNote())
						s |= ScStyle_Superscript;
					else
						s &= ~ScStyle_Superscript;
				}
				if (s != item->itemText.charStyle(index).effects())
				{
					CharStyle haveSuperscript;
					haveSuperscript.setFeatures(s.featureList());
					item->itemText.applyCharStyle(index, 1, haveSuperscript);
				}
			}
		}

		bool bullet = false;
		if (index == 0 || item->itemText.text(index - 1) == SpecialChars::PARSEP)
		{
			const ParagraphStyle& style = item->itemText.paragraphStyle(index);
			if (style.hasBullet() || style.hasNum())
			{
				bullet = true;
				if (mark == NULL || !mark->isType(MARKBullNumType))
				{
					item->itemText.insertMark(new BulNumMark(), index);
					index--;
					continue;
				}
				if (style.hasBullet())
					mark->setString(style.bulletStr());
				else if (style.hasNum() && mark->getString().isEmpty())
				{
					mark->setString("?");
					item->doc()->flag_Renumber = true;
				}
			}
		}

		if (!bullet && mark && mark->isType(MARKBullNumType))
		{
			item->itemText.removeChars(index, 1);
			index--;
			continue;
		}

	}

}

void PropertyWidget_ParEffect::updateStyle(const ParagraphStyle& newPStyle)
{
	if (peCombo->currentIndex() && !newPStyle.hasBullet() && !newPStyle.hasDropCap() && !newPStyle.hasNum())
	{
		enableParEffect(false);
		return;
	}

	QSignalBlocker blocker1(peCombo);
	QSignalBlocker blocker2(dropCapLines);
	QSignalBlocker blocker3(bulletStrEdit);
	QSignalBlocker blocker4(numComboBox);
	QSignalBlocker blocker5(numLevelSpin);
	QSignalBlocker blocker6(numFormatCombo);
	QSignalBlocker blocker7(numPrefix);
	QSignalBlocker blocker8(numSuffix);
	QSignalBlocker blocker9(numStart);
	QSignalBlocker blockerA(peOffset);
	QSignalBlocker blockerB(peIndent);
	QSignalBlocker blockerC(peCharStyleCombo);

	bool enablePE = true;
	if (newPStyle.hasDropCap())
	{
		peCombo->setCurrentIndex(1);
		enableDropCap(true);
	}
	else if (newPStyle.hasBullet())
	{
		peCombo->setCurrentIndex(2);
		enableBullet(true);
	}
	else if (newPStyle.hasNum())
	{
		peCombo->setCurrentIndex(3);
		enableNum(true);
	}
	else
		enablePE = false;

	QString numName = numComboBox->currentText();
	int nFormat = 0;
	dropCapLines->setValue(newPStyle.dropCapLines());
	bulletStrEdit->setEditText(newPStyle.bulletStr());
	numName = newPStyle.numName();
	if (numName == "")
		numName = "<local block>";
	numComboBox->setCurrentIndex(numComboBox->findText(numName));
	NumStruct * numS = m_doc->numerations.value(numName);
	if (numS)
		numLevelSpin->setMaximum(numS->m_counters.count()+1);
	else
		numLevelSpin->setMaximum(3);
	numLevelSpin->setValue(newPStyle.numLevel() +1);
	numPrefix->setText(newPStyle.numPrefix());
	numSuffix->setText(newPStyle.numSuffix());
	numStart->setValue(newPStyle.numStart());

	nFormat = newPStyle.numFormat();
	numFormatCombo->setCurrentIndex(nFormat);
	peOffset->setValue(newPStyle.parEffectOffset() * m_unitRatio);
	peIndent->setChecked(newPStyle.parEffectIndent());
	showCharStyle(newPStyle.peCharStyleName());

	enableParEffect(enablePE);
}

void PropertyWidget_ParEffect::connectSignals()
{
	connect(peCombo, SIGNAL(activated(int)), this, SLOT(handleParEffectUse()), Qt::UniqueConnection);
	connect(dropCapLines, SIGNAL(valueChanged(int)), this, SLOT(handleDropCapLines(int)), Qt::UniqueConnection);
	connect(bulletStrEdit, SIGNAL(editTextChanged(QString)), this, SLOT(handleBulletStr(QString)), Qt::UniqueConnection);
	connect(numComboBox, SIGNAL(activated(QString)), this, SLOT(handleNumName(QString)), Qt::UniqueConnection);
	connect(numLevelSpin, SIGNAL(valueChanged(int)), this, SLOT(handleNumLevel(int)), Qt::UniqueConnection);
	connect(numFormatCombo, SIGNAL(activated(int)), this, SLOT(handleNumFormat(int)), Qt::UniqueConnection);
	connect(numPrefix, SIGNAL(textChanged(QString)), this, SLOT(handleNumPrefix(QString)), Qt::UniqueConnection);
	connect(numSuffix, SIGNAL(textChanged(QString)), this, SLOT(handleNumSuffix(QString)), Qt::UniqueConnection);
	connect(numStart, SIGNAL(valueChanged(int)), this, SLOT(handleNumStart(int)), Qt::UniqueConnection);
	connect(peOffset, SIGNAL(valueChanged(double)), this, SLOT(handlePEOffset(double)), Qt::UniqueConnection);
	connect(peIndent, SIGNAL(toggled(bool)), this, SLOT(handlePEIndent(bool)), Qt::UniqueConnection);
	connect(peCharStyleCombo, SIGNAL(newStyle(QString)), this, SLOT(handlePECharStyle(QString)), Qt::UniqueConnection);
}

void PropertyWidget_ParEffect::disconnectSignals()
{
	disconnect(peCombo, SIGNAL(activated(int)), this, SLOT(handleParEffectUse()));
	disconnect(dropCapLines, SIGNAL(valueChanged(int)), this, SLOT(handleDropCapLines(int)));
	disconnect(bulletStrEdit, SIGNAL(editTextChanged(QString)), this, SLOT(handleBulletStr(QString)));
	disconnect(numComboBox, SIGNAL(activated(QString)), this, SLOT(handleNumName(QString)));
	disconnect(numLevelSpin, SIGNAL(valueChanged(int)), this, SLOT(handleNumLevel(int)));
	disconnect(numFormatCombo, SIGNAL(activated(int)), this, SLOT(handleNumFormat(int)));
	disconnect(numPrefix, SIGNAL(textChanged(QString)), this, SLOT(handleNumPrefix(QString)));
	disconnect(numSuffix, SIGNAL(textChanged(QString)), this, SLOT(handleNumSuffix(QString)));
	disconnect(numStart, SIGNAL(valueChanged(int)), this, SLOT(handleNumStart(int)));
	disconnect(peOffset, SIGNAL(valueChanged(double)), this, SLOT(handlePEOffset(double)));
	disconnect(peIndent, SIGNAL(toggled(bool)), this, SLOT(handlePEIndent(bool)));
	disconnect(peCharStyleCombo, SIGNAL(newStyle(QString)), this, SLOT(handlePECharStyle(QString)));
}

void PropertyWidget_ParEffect::configureWidgets(void)
{
	bool enabled = false;
	if (m_item && m_doc)
	{
		PageItem_TextFrame *textItem = m_item->asTextFrame();
		if (m_doc->appMode == modeEditTable)
			textItem = m_item->asTable()->activeCell().textFrame();
		if (textItem || m_item->asPathText())
			enabled = true;
	}
	setEnabled(enabled);
}

void PropertyWidget_ParEffect::handleAppModeChanged(int oldMode, int mode)
{
	if (oldMode == modeEditTable || mode == modeEditTable)
	{
		setCurrentItem(m_item);
	}
}

void PropertyWidget_ParEffect::handleSelectionChanged()
{
	if (!m_doc || !m_ScMW || m_ScMW->scriptIsRunning())
		return;

	PageItem* currItem = currentItemFromSelection();
	setCurrentItem(currItem);
	updateGeometry();
}

void PropertyWidget_ParEffect::handleUpdateRequest(int updateFlags)
{
	if (updateFlags & reqCharStylesUpdate)
		updateCharStyles();
	if (updateFlags & reqStyleComboDocUpdate)
		setDoc(m_doc ? m_doc : 0);
	if (updateFlags & reqNumUpdate)
		fillNumerationsCombo();
}

void PropertyWidget_ParEffect::handleParEffectUse()
{
	if (!m_doc || !m_item)
		return;
	ParagraphStyle newStyle;
	enableParEffect(peCombo->currentIndex() != 0);
	if (peCombo->currentIndex() == 1)
	{
		enableDropCap(true);
		newStyle.setDropCapLines(dropCapLines->value());
		newStyle.setHasDropCap(true);
		newStyle.setHasBullet(false);
		newStyle.setHasNum(false);
	}
	else if (peCombo->currentIndex() == 2)
	{
		enableBullet(true);
		newStyle.setHasBullet(true);
		QString bStr = bulletStrEdit->currentText();
		if (bStr.isEmpty())
			bStr = QChar(0x2022);
		newStyle.setBulletStr(bStr);
		newStyle.setHasNum(false);
		newStyle.setHasDropCap(false);
	}
	else if (peCombo->currentIndex() == 3)
	{
		enableNum(true);
		newStyle.setHasDropCap(false);
		newStyle.setHasBullet(false);
		newStyle.setHasNum(true);
		newStyle.setNumName(numComboBox->currentText());
		newStyle.setNumFormat(numFormatCombo->currentIndex());
		newStyle.setNumLevel(numLevelSpin->value() -1);
		newStyle.setNumStart(numStart->value());
		newStyle.setNumPrefix(numPrefix->text());
		newStyle.setNumSuffix(numSuffix->text());
		if (newStyle.numName() == "<local block>")
		{
			newStyle.setNumOther(true);
			newStyle.setNumHigher(true);
		}
	}
	else
	{
		newStyle.setHasDropCap(false);
		newStyle.setHasBullet(false);
		newStyle.setHasNum(false);
	}
	newStyle.setParEffectOffset(peOffset->value() / m_unitRatio);
	newStyle.setParEffectIndent(peIndent->isChecked());
	handleChanges(m_item, newStyle);
	insertMarks(m_item);
	m_doc->updateNumbers();

}

void PropertyWidget_ParEffect::handleBulletStr(QString bulStr)
{
	if (!m_doc || !m_item)
		return;
	ParagraphStyle newStyle;
	if (bulStr.isEmpty())
		bulStr = QChar(0x2022);
	newStyle.setBulletStr(bulStr);
	handleChanges(m_item, newStyle);
}

void PropertyWidget_ParEffect::handleDropCapLines(int dcLines)
{
	if (!m_doc || !m_item)
		return;
	ParagraphStyle newStyle;
	newStyle.setDropCapLines(dcLines);
	handleChanges(m_item, newStyle);
}

void PropertyWidget_ParEffect::handleNumName(QString numName)
{
	if (!m_doc || !m_item)
		return;
	disconnectSignals();
	ParagraphStyle newStyle;
	if (numName == "<local block>")
	{
		newStyle.setNumOther(true);
		newStyle.setNumHigher(true);
		newStyle.setNumRestart(NSRstory);
	}
	else
	{
		NumStruct * numS = m_doc->numerations.value(numName);
		Q_ASSERT(numS);
		int level = qMin(numLevelSpin->value(), numS->m_counters.count()) -1;
		numLevelSpin->setValue(level +1);
		newStyle.setNumLevel(level);
		Numeration num = numS->m_nums[level];
		numFormatCombo->setCurrentIndex((int) num.numFormat);
		numStart->setValue(num.start);
		numPrefix->setText(num.prefix);
		numSuffix->setText(num.suffix);
	}
	newStyle.setNumPrefix(numPrefix->text());
	newStyle.setNumSuffix(numSuffix->text());
	newStyle.setNumName(numName);
	newStyle.setNumFormat((NumFormat) numFormatCombo->currentIndex());
	handleChanges(m_item, newStyle);
	connectSignals();
}

void PropertyWidget_ParEffect::handleNumFormat(int style)
{
	if (!m_doc || !m_item)
		return;
	ParagraphStyle newStyle;
	newStyle.setNumFormat(style);
	handleChanges(m_item, newStyle);
}

void PropertyWidget_ParEffect::handleNumLevel(int level)
{
	if (!m_doc || !m_item)
		return;
	if ((numComboBox->currentText() != "<local block>") && level == numLevelSpin->maximum())
	{
		NumStruct * numS = m_doc->numerations.value(numComboBox->currentText());
		Q_ASSERT(numS);
		while (level > numS->m_counters.count())
		{
			numS->m_counters.append(0);
			Numeration num;
			num.numFormat = (NumFormat) numFormatCombo->currentIndex();
			num.prefix = numPrefix->text();
			num.suffix = numSuffix->text();
			num.start = numStart->value();
			numS->m_nums.append(num);
		}
	}
	ParagraphStyle newStyle;
	newStyle.setNumLevel(level -1);
	handleChanges(m_item, newStyle);
}

void PropertyWidget_ParEffect::handleNumPrefix(QString prefix)
{
	if (!m_doc || !m_item)
		return;
	ParagraphStyle newStyle;
	newStyle.setNumPrefix(prefix);
	handleChanges(m_item, newStyle);
}

void PropertyWidget_ParEffect::handleNumSuffix(QString suffix)
{
	if (!m_doc || !m_item)
		return;
	ParagraphStyle newStyle;
	newStyle.setNumSuffix(suffix);
	handleChanges(m_item, newStyle);
}

void PropertyWidget_ParEffect::handleNumStart(int start)
{
	if (!m_doc || !m_item)
		return;
	ParagraphStyle newStyle;
	newStyle.setNumStart(start);
	handleChanges(m_item, newStyle);
}

void PropertyWidget_ParEffect::handlePEOffset(double offset)
{
	if (!m_doc || !m_item)
		return;
	ParagraphStyle newStyle;
	newStyle.setParEffectOffset(offset / m_unitRatio);
	handleChanges(m_item, newStyle);
}

void PropertyWidget_ParEffect::handlePEIndent(bool indent)
{
	if (!m_doc || !m_item)
		return;
	ParagraphStyle newStyle;
	newStyle.setParEffectIndent(indent);
	handleChanges(m_item, newStyle);
}

void PropertyWidget_ParEffect::handlePECharStyle(QString name)
{
	if (!m_doc || !m_item)
		return;
	ParagraphStyle newStyle;
	newStyle.setPeCharStyleName(name);
	handleChanges(m_item, newStyle);
}

void PropertyWidget_ParEffect::changeEvent(QEvent *e)
{
	if (e->type() == QEvent::LanguageChange)
	{
		languageChange();
		return;
	}
	QWidget::changeEvent(e);
}

void PropertyWidget_ParEffect::handleChanges(PageItem *item, ParagraphStyle &newStyle)
{
	if (m_doc->appMode == modeEditTable)
		item = item->asTable()->activeCell().textFrame();
	if (item != NULL)
	{
		disconnect(m_doc->m_Selection, SIGNAL(selectionChanged()), this, SLOT(handleSelectionChanged()));
		disconnect(m_doc             , SIGNAL(docChanged())      , this, SLOT(handleSelectionChanged()));

		Selection tempSelection(this, false);
		tempSelection.addItem(item, true);
		m_doc->itemSelection_ApplyParagraphStyle(newStyle, &tempSelection);
		m_doc->updateNumbers();

		connect(m_doc->m_Selection, SIGNAL(selectionChanged()), this, SLOT(handleSelectionChanged()));
		connect(m_doc             , SIGNAL(docChanged())      , this, SLOT(handleSelectionChanged()));
	}
}

void PropertyWidget_ParEffect::languageChange()
{
	fillPECombo();
	retranslateUi(this);
}

void PropertyWidget_ParEffect::openEnhanced()
{
	if (m_enhanced)
		return;

	QApplication::changeOverrideCursor(QCursor(Qt::WaitCursor));
	m_enhanced = new CharSelectEnhanced(this);
	m_enhanced->setModal(true);
	connect(m_enhanced, SIGNAL(insertSpecialChars(const QString &)), this, SLOT(insertSpecialChars(const QString &)));
	connect(m_enhanced, SIGNAL(paletteShown(bool)), bulletCharTableButton, SLOT(setChecked(bool)));
	m_enhanced->setDoc(m_doc);
	m_enhanced->setEnabled(true);
	QString styleName = peCharStyleCombo->currentText();
	setCurrentComboItem(m_enhanced->fontSelector, m_item->currentStyle().charStyle().font().scName());
	m_enhanced->newFont(m_enhanced->fontSelector->currentIndex());
	m_enhanced->show();
	QApplication::changeOverrideCursor(Qt::ArrowCursor);
}

void PropertyWidget_ParEffect::closeEnhanced(bool show)
{
	if (!m_enhanced || show)
		return;
	disconnect(m_enhanced, SIGNAL(insertSpecialChars(const QString &)), this, SLOT(insertSpecialChars(const QString &)));
	disconnect(m_enhanced, SIGNAL(paletteShown(bool)), bulletCharTableButton, SLOT(setChecked(bool)));
	m_enhanced->close();
	delete m_enhanced;
	m_enhanced = NULL;
}

void PropertyWidget_ParEffect::on_bulletCharTableButton_toggled(bool checked)
{
	if (m_enhanced && !checked)
		closeEnhanced();
	else if (!m_enhanced && checked)
		openEnhanced();
}
void PropertyWidget_ParEffect::insertSpecialChars(const QString &chars)
{
	bulletStrEdit->lineEdit()->setText(chars);
}
