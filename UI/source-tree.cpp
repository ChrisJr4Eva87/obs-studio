#include "window-basic-main.hpp"
#include "obs-app.hpp"
#include "source-tree.hpp"
#include "qt-wrappers.hpp"
#include "visibility-checkbox.hpp"
#include "locked-checkbox.hpp"
#include "expand-checkbox.hpp"
#include "visibility-item-widget.hpp"

#include <obs-frontend-api.h>
#include <obs.h>

#include <QLabel>
#include <QLineEdit>
#include <QSpacerItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>

#include <QStylePainter>
#include <QStyleOptionFocusRect>

/* ========================================================================= */

SourceTreeItem::SourceTreeItem(SourceTree *tree_, OBSSceneItem sceneitem_)
	: tree         (tree_),
	  sceneitem    (sceneitem_)
{
	setAttribute(Qt::WA_TranslucentBackground);

	obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	const char *name = obs_source_get_name(source);

	spacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);

	expand = new ExpandCheckBox();
	expand->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	expand->setMaximumSize(16, 16);
	expand->setIconSize(QSize(16, 16));

	vis = new VisibilityCheckBox();
	vis->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	vis->setMaximumSize(16, 16);

	lock = new LockedCheckBox();
	lock->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	lock->setMaximumSize(16, 16);

	label = new QLabel(QT_UTF8(name));
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	label->setAttribute(Qt::WA_TranslucentBackground);

	boxLayout = new QHBoxLayout();
	boxLayout->setSpacing(1);
	boxLayout->setContentsMargins(0, 0, 0, 0);
	boxLayout->addSpacerItem(spacer);
	boxLayout->addWidget(expand);
	boxLayout->addWidget(vis);
	boxLayout->addWidget(lock);
	boxLayout->addWidget(label);

	RecalculateSpacing();

	setLayout(boxLayout);
}

void SourceTreeItem::RecalculateSpacing()
{
	int indentation = 0;

	//TODO

	spacer->changeSize(indentation, 0,
			QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void SourceTreeItem::mousePressEvent(QMouseEvent *event)
{
	QWidget::mousePressEvent(event);
}

void SourceTreeItem::mouseMoveEvent(QMouseEvent *event)
{
	QWidget::mouseMoveEvent(event);
}

void SourceTreeItem::mouseReleaseEvent(QMouseEvent *event)
{
	QWidget::mouseReleaseEvent(event);
}

void SourceTreeItem::paintEvent(QPaintEvent *event)
{
	static int chi = 0;

	bool active = false;
	bool selected = true;
#if defined(_WIN32) || defined(__APPLE__)
	QPalette::ColorGroup group = active ?
		QPalette::Active : QPalette::Inactive;
#else
	QPalette::ColorGroup group = QPalette::Active;
#endif

#ifdef _WIN32
	QPalette::ColorRole highlightTextRole = QPalette::WindowText;
#else
	QPalette::ColorRole highlightTextRole = QPalette::HighlightedText;
#endif

	QPalette::ColorRole bkRole = selected ?
		QPalette::Highlight : QPalette::Base;

	QPalette::ColorRole textRole = (selected && active) ?
		highlightTextRole : QPalette::WindowText;

	QPalette pal = palette();

	QColor textColor = pal.color(group, textRole);
	QBrush bkColor = pal.brush(group, bkRole);

	pal.setColor(QPalette::WindowText, textColor);

	/* ----------------------------- */
	/* set widget colors             */

	//vis->setPalette(pal);
	//label->setStyleSheet(QString("color: %1;").arg(textColor.name()));

	QWidget::paintEvent(event);
}

void SourceTreeItem::EnterEditMode()
{
	setFocusPolicy(Qt::StrongFocus);
	boxLayout->removeWidget(label);
	editor = new QLineEdit(label->text());
	editor->installEventFilter(this);
	boxLayout->addWidget(editor);
	setFocusProxy(editor);
}

void SourceTreeItem::ExitEditMode(bool save)
{
	if (!editor)
		return;

	if (save) {
		SignalBlocker sourcesSignalBlocker(this);
		obs_source_t *source = obs_sceneitem_get_source(sceneitem);
		obs_source_set_name(source, QT_TO_UTF8(editor->text()));
		label->setText(editor->text());
	}

	setFocusProxy(nullptr);
	boxLayout->removeWidget(editor);
	delete editor;
	editor = nullptr;
	setFocusPolicy(Qt::NoFocus);
	boxLayout->addWidget(label);
}

bool SourceTreeItem::eventFilter(QObject *object, QEvent *event)
{
	if (editor != object)
		return false;

	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

		if (keyEvent->matches(QKeySequence::Cancel)) {
			QMetaObject::invokeMethod(this, "ExitEditMode",
					Qt::QueuedConnection,
					Q_ARG(bool, false));
			return true;
		}

		switch (keyEvent->key()) {
		case Qt::Key_Tab:
		case Qt::Key_Backtab:
		case Qt::Key_Enter:
		case Qt::Key_Return:
			QMetaObject::invokeMethod(this, "ExitEditMode",
					Qt::QueuedConnection,
					Q_ARG(bool, true));
			return true;
		}
	} else if (event->type() == QEvent::FocusOut) {
		QMetaObject::invokeMethod(this, "ExitEditMode",
				Qt::QueuedConnection,
				Q_ARG(bool, false));
		return true;
	}

	return false;
}

/* ========================================================================= */

void SourceTreeModel::OBSFrontendEvent(enum obs_frontend_event event, void *ptr)
{
	SourceTreeModel *stm = reinterpret_cast<SourceTreeModel *>(ptr);

	switch ((int)event) {
	case OBS_FRONTEND_EVENT_SCENE_CHANGED: /* TODO studio mode */
		stm->SceneChanged();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		stm->Clear();
		break;
	}
}

void SourceTreeModel::Clear()
{
	beginResetModel();
	items.clear();
	endResetModel();
}

void SourceTreeModel::SceneChanged()
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();

	auto addItem = [this] (obs_sceneitem_t *sceneitem)
	{
		SourceTreeItem *item = new SourceTreeItem(st, sceneitem);
		items << item;
	};

	using addItem_t = decltype(addItem);

	auto preAddItem = [] (obs_scene_t*, obs_sceneitem_t *item, void *data)
	{
		(*static_cast<addItem_t*>(data))(item);
		return true;
	};

	beginResetModel();
	items.clear();
	obs_scene_enum_items(scene, preAddItem, &addItem);
	endResetModel();

	st->ResetWidgets();
}

/* reorders list optimally with model reorder funcs */
void SourceTreeModel::ReorderItems()
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();

	QVector<OBSSceneItem> newitems;
	auto enumItem = [] (obs_scene_t*, obs_sceneitem_t *item, void *ptr)
	{
		QVector<OBSSceneItem> &newitems =
			*reinterpret_cast<QVector<OBSSceneItem>*>(ptr);
		newitems << item;
		return true;
	};

	obs_scene_enum_items(scene, enumItem, &newitems);

	/* if item list has changed size, do full reset */
	if (newitems.count() != items.count()) {
		SceneChanged();
		return;
	}

	for (;;) {
		int idx1Old = 0;
		int idx1New = 0;
		int count;
		int i;

		/* find first starting changed item index */
		for (i = 0; i < newitems.count(); i++) {
			obs_sceneitem_t *oldItem = items[i]->sceneitem;
			obs_sceneitem_t *newItem = newitems[i];
			if (oldItem != newItem)
				idx1Old = i;
		}

		/* if everything is the same, break */
		if (i == newitems.count()) {
			break;
		}

		/* find new starting index */
		for (i = idx1Old + 1; i < newitems.count(); i++) {
			obs_sceneitem_t *oldItem = items[idx1Old]->sceneitem;
			obs_sceneitem_t *newItem = newitems[i];

			if (oldItem == newItem) {
				idx1New = i;
				break;
			}
		}

		/* if item could not be found, do full reset */
		if (i == newitems.count()) {
			SceneChanged();
			return;
		}

		/* get move count */
		for (count = 1; (idx1New + count) < newitems.count(); count++) {
			int oldIdx = idx1Old + count;
			int newIdx = idx1New + count;

			obs_sceneitem_t *oldItem = items[oldIdx]->sceneitem;
			obs_sceneitem_t *newItem = newitems[newIdx];

			if (oldItem != newItem) {
				break;
			}
		}

		/* move items */
		beginMoveRows(QModelIndex(), idx1Old, idx1Old + count - 1,
		              QModelIndex(), idx1New + count);
		for (i = 0; i < count; i++)
			items.move(idx1Old, idx1New + count);
		endMoveRows();
	}
}

void SourceTreeModel::Add(obs_sceneitem_t *item)
{
	SourceTreeItem *widget = new SourceTreeItem(st, item);
	beginInsertRows(QModelIndex(), 0, 0);
	items.insert(0, widget);
	endInsertRows();

	st->UpdateWidget(createIndex(0, 0, nullptr), widget);
}

void SourceTreeModel::Remove(obs_sceneitem_t *item)
{
	for (int i = 0; i < items.count(); i++) {
		if (items[i]->sceneitem == item) {
			beginRemoveRows(QModelIndex(), i, i);
			items.remove(i);
			endRemoveRows();
			break;
		}
	}
}

OBSSceneItem SourceTreeModel::Get(int idx)
{
	if (idx == -1 || idx >= items.count())
		return OBSSceneItem();
	return items[idx]->sceneitem;
}

SourceTreeModel::SourceTreeModel(SourceTree *st_)
	: QAbstractListModel (nullptr),
	  st                 (st_)
{
	obs_frontend_add_event_callback(OBSFrontendEvent, this);
}

SourceTreeModel::~SourceTreeModel()
{
	obs_frontend_remove_event_callback(OBSFrontendEvent, this);
}

int SourceTreeModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : items.count();
}

QVariant SourceTreeModel::data(const QModelIndex &index, int role) const
{
	return QVariant();
}

Qt::ItemFlags SourceTreeModel::flags(const QModelIndex &index) const
{
	if (index.row() < 0 || index.row() >= items.count())
		return 0;

	return Qt::ItemIsSelectable |
	       Qt::ItemIsEnabled |
	       Qt::ItemIsEditable |
	       Qt::ItemIsDragEnabled |
	       Qt::ItemIsDropEnabled;
}

/* ========================================================================= */

SourceTree::SourceTree(QWidget *parent_) : QListView(parent_)
{
	SourceTreeModel *stm_ = new SourceTreeModel(this);
	setModel(stm_);
}

void SourceTree::ResetWidgets()
{
	SourceTreeModel *stm = GetStm();
	for (int i = 0; i < stm->items.count(); i++) {
		QModelIndex index = stm->createIndex(i, 0, nullptr);
		setIndexWidget(index, stm->items[i]);
	}
}

void SourceTree::UpdateWidget(const QModelIndex &idx, QWidget *widget)
{
	setIndexWidget(idx, widget);
}

void SourceTree::SelectItem(obs_sceneitem_t *sceneitem, bool select)
{
	SourceTreeModel *stm = GetStm();
	int i = 0;

	for (; i < stm->items.count(); i++) {
		if (stm->items[i]->sceneitem == sceneitem)
			break;
	}

	if (i == stm->items.count())
		return;

	QModelIndex index = stm->createIndex(i, 0);
	if (index.isValid())
		selectionModel()->select(index, select
				? QItemSelectionModel::Select
				: QItemSelectionModel::Deselect);
}

Q_DECLARE_METATYPE(OBSSceneItem);

void SourceTree::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
		QListView::mouseDoubleClickEvent(event);
}

void SourceTree::dropEvent(QDropEvent *event)
{
	QListView::dropEvent(event);
	if (!event->isAccepted())
		return;

	/* TODO */
}

void SourceTree::selectionChanged(
		const QItemSelection &selected,
		const QItemSelection &deselected)
{
	{
		SignalBlocker sourcesSignalBlocker(this);
		SourceTreeModel *stm = GetStm();

		QModelIndexList selectedIdxs = selected.indexes();
		QModelIndexList deselectedIdxs = deselected.indexes();

		for (int i = 0; i < selectedIdxs.count(); i++) {
			int idx = selectedIdxs[i].row();
			obs_sceneitem_select(stm->items[idx]->sceneitem, true);
		}

		for (int i = 0; i < deselectedIdxs.count(); i++) {
			int idx = deselectedIdxs[i].row();
			obs_sceneitem_select(stm->items[idx]->sceneitem, false);
		}
	}
	QListView::selectionChanged(selected, deselected);
}

void SourceTree::Edit(int row)
{
	SourceTreeModel *stm = GetStm();
	if (row < 0 || row >= stm->items.count())
		return;

	SourceTreeItem *item = stm->items[row];
	item->EnterEditMode();
	edit(stm->createIndex(row, 0));
}
