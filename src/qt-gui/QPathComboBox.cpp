#include "QPathComboBox.moc"

#include <QTimer>
#include <QDirModel>
#include <QCompleter>
#include <QLineEdit>
#include <QAbstractItemView>

#include <boost/foreach.hpp>

#include "../util/Filesystem.h"

#include <iostream>

QPathComboBox::QPathComboBox(QWidget* parent)
: QComboBox(parent)
{
	signalblocked = false;
	matchflags = Qt::MatchExactly; // Qt::MatchCaseSensitive;
	this->setInsertPolicy(QComboBox::InsertAtTop);

	QObject::connect(this, SIGNAL(editTextChanged(QString)), this, SLOT(onEditTextChanged(QString)));

	QTimer::singleShot(0, this, SLOT(init()));
}

void QPathComboBox::init()
{
	QCompleter* completer = new QCompleter(this);
	QDirModel* dirmodel = new QDirModel(completer);
	dirmodel->setFilter(dirmodel->filter() & ~QDir::Files); // don't list files
	completer->setModel(dirmodel);
	this->setCompleter(completer);
}

void QPathComboBox::focusInEvent(QFocusEvent* event)
{
	QComboBox::focusInEvent(event);
	updateValidStatus();
}

void QPathComboBox::focusOutEvent(QFocusEvent* event)
{
	QComboBox::focusOutEvent(event);
	updateValidStatus();
}

void QPathComboBox::onEditTextChanged(QString path)
{
	if (signalblocked) return;
	updateValidStatus();
	currentPathChanged(path);
}

void QPathComboBox::updateValidStatus()
{
	bool isvalid = this->completer()->popup()->isVisible()
	            || ext::filesystem::exists(boost::filesystem::path(currentPath().toStdString()));

	if (isvalid)
		this->setStyleSheet("");
	else
		this->setStyleSheet("* { color: black; background-color: #ffb3b3; }");
}

void QPathComboBox::setRecentPaths(QStringList paths)
{
	signalblocked = true;
	this->clear();
	this->addItems(paths);
	signalblocked = false;
}

QStringList QPathComboBox::recentPaths(size_t max)
{
	addRecentPath(currentPath());
	QStringList r;
	size_t c = count();
	for (size_t i = 0; i < c && i < max; ++i)
		r << this->itemText(i);
	return r;
}

void QPathComboBox::setRecentPaths(const std::vector<std::string>& paths)
{
	signalblocked = true;
	this->clear();
	BOOST_FOREACH(const std::string& s, paths)
		this->addItem(QString::fromStdString(s));
	signalblocked = false;
}

std::vector<std::string> QPathComboBox::recentPathsStd(size_t max)
{
	addRecentPath(currentPath());
	std::vector<std::string> r;
	size_t c = count();
	for (size_t i = 0; i < c && i < max; ++i)
		r.push_back(this->itemText(i).toStdString());
	return r;
}

void QPathComboBox::addRecentPath(QString path)
{
	signalblocked = true;
	int idx = this->findText(path, matchflags);
	bool dosignal = (idx != this->currentIndex());
	if (idx != -1)
		this->removeItem(idx);
	this->insertItem(0, path);
	this->setCurrentIndex(0);
	signalblocked = false;
	if (dosignal) currentPathChanged(path);
}

void QPathComboBox::addPath(QString path)
{
	signalblocked = true;
	if (this->findText(path, matchflags) == -1)
		this->addItem(path);
	signalblocked = false;
}

void QPathComboBox::setCurrentPath(QString path)
{
	this->setEditText(path);
}

QString QPathComboBox::currentPath()
{
	return this->currentText();
}