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

static inline OBSScene GetCurrentScene()
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	return main->GetCurrentScene();
}

/* ========================================================================= */

SourceTreeItem::SourceTreeItem(SourceTree *tree_, OBSSceneItem sceneitem_)
	: tree         (tree_),
	  sceneitem    (sceneitem_)
{
	setAttribute(Qt::WA_TranslucentBackground);

	obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	const char *name = obs_source_get_name(source);

	vis = new VisibilityCheckBox();
	vis->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	vis->setMaximumSize(16, 16);
	vis->setChecked(obs_sceneitem_visible(sceneitem));

	lock = new LockedCheckBox();
	lock->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	lock->setMaximumSize(16, 16);
	lock->setChecked(obs_sceneitem_locked(sceneitem));

	label = new QLabel(QT_UTF8(name));
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	label->setAttribute(Qt::WA_TranslucentBackground);

	boxLayout = new QHBoxLayout();
	boxLayout->setContentsMargins(1, 1, 2, 1);
	boxLayout->setSpacing(1);
	boxLayout->addWidget(label);
	boxLayout->addWidget(vis);
	boxLayout->addWidget(lock);

	Update(false);

	setLayout(boxLayout);

	/* --------------------------------------------------------- */

	auto setItemVisible = [this] (bool checked)
	{
		SignalBlocker sourcesSignalBlocker(this);
		obs_sceneitem_set_visible(sceneitem, checked);
	};

	auto setItemLocked = [this] (bool checked)
	{
		SignalBlocker sourcesSignalBlocker(this);
		obs_sceneitem_set_locked(sceneitem, checked);
	};

	connect(vis, &QAbstractButton::clicked, setItemVisible);
	connect(lock, &QAbstractButton::clicked, setItemLocked);
}

void SourceTreeItem::DisconnectSignals()
{
	sceneRemoveSignal.Disconnect();
	itemRemoveSignal.Disconnect();
	visibleSignal.Disconnect();
	renameSignal.Disconnect();
	removeSignal.Disconnect();
}

void SourceTreeItem::ReconnectSignals()
{
	if (!sceneitem)
		return;

	DisconnectSignals();

	/* --------------------------------------------------------- */

	auto removeItem = [] (void *data, calldata_t *cd)
	{
		SourceTreeItem *this_ = reinterpret_cast<SourceTreeItem*>(data);
		obs_sceneitem_t *curItem =
			(obs_sceneitem_t*)calldata_ptr(cd, "item");

		if (!curItem || curItem == this_->sceneitem) {
			this_->DisconnectSignals();
			this_->sceneitem = nullptr;
		}
	};

	auto itemVisible = [] (void *data, calldata_t *cd)
	{
		SourceTreeItem *this_ = reinterpret_cast<SourceTreeItem*>(data);
		obs_sceneitem_t *curItem =
			(obs_sceneitem_t*)calldata_ptr(cd, "item");
		bool visible = calldata_bool(cd, "visible");

		if (curItem == this_->sceneitem)
			QMetaObject::invokeMethod(this_, "VisibilityChanged",
					Q_ARG(bool, visible));
	};

	obs_scene_t *scene = obs_sceneitem_get_scene(sceneitem);
	obs_source_t *sceneSource = obs_scene_get_source(scene);
	signal_handler_t *signal = obs_source_get_signal_handler(sceneSource);

	sceneRemoveSignal.Connect(signal, "remove", removeItem, this);
	itemRemoveSignal.Connect(signal, "item_remove", removeItem, this);
	visibleSignal.Connect(signal, "item_visible", itemVisible, this);

	/* --------------------------------------------------------- */

	auto renamed = [] (void *data, calldata_t *cd)
	{
		SourceTreeItem *this_ = reinterpret_cast<SourceTreeItem*>(data);
		const char *name = calldata_string(cd, "new_name");

		QMetaObject::invokeMethod(this_, "Renamed",
				Q_ARG(QString, QT_UTF8(name)));
	};

	auto removeSource = [] (void *data, calldata_t *cd)
	{
		SourceTreeItem *this_ = reinterpret_cast<SourceTreeItem*>(data);
		this_->DisconnectSignals();
		this_->sceneitem = nullptr;
	};

	obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	signal = obs_source_get_signal_handler(source);
	renameSignal.Connect(signal, "rename", renamed, this);
	removeSignal.Connect(signal, "remove", removeSource, this);
}

void SourceTreeItem::mouseDoubleClickEvent(QMouseEvent *event)
{
	QWidget::mouseDoubleClickEvent(event);

	if (expand) {
		expand->setChecked(!expand->isChecked());
	} else {
		obs_source_t *source = obs_sceneitem_get_source(sceneitem);
		OBSBasic *main =
			reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
		if (source) {
			main->CreatePropertiesWindow(source);
		}
	}
}

void SourceTreeItem::EnterEditMode()
{
	setFocusPolicy(Qt::StrongFocus);
	boxLayout->removeWidget(label);
	editor = new QLineEdit(label->text());
	editor->installEventFilter(this);
	boxLayout->insertWidget(1, editor);
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
	boxLayout->insertWidget(1, label);
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

void SourceTreeItem::VisibilityChanged(bool visible)
{
	vis->setChecked(visible);
}

void SourceTreeItem::Renamed(const QString &name)
{
	label->setText(name);
}

void SourceTreeItem::Update(bool force)
{
	OBSScene scene = GetCurrentScene();
	obs_scene_t *itemScene = obs_sceneitem_get_scene(sceneitem);

	Type newType;

	/* ------------------------------------------------- */
	/* if it's a group item, insert group checkbox       */

	if (!!obs_sceneitem_group_from_item(sceneitem)) {
		newType = Type::Group;

	/* ------------------------------------------------- */
	/* if it's a group sub-item                          */

	} else if (itemScene != scene) {
		newType = Type::SubItem;

	/* ------------------------------------------------- */
	/* if it's a regular item                            */

	} else {
		newType = Type::Item;
	}

	/* ------------------------------------------------- */

	if (!force && newType == type) {
		return;
	}

	/* ------------------------------------------------- */

	ReconnectSignals();

	if (spacer) {
		boxLayout->removeItem(spacer);
		delete spacer;
		spacer = nullptr;
	}

	if (type == Type::Group) {
		boxLayout->removeWidget(expand);
		expand->deleteLater();
		expand = nullptr;
	}

	type = newType;

	if (type == Type::SubItem) {
		spacer = new QSpacerItem(16, 1);
		boxLayout->insertItem(0, spacer);

	} else if (type == Type::Group) {
		expand = new SourceTreeSubItemCheckBox();
		expand->setSizePolicy(
				QSizePolicy::Maximum,
				QSizePolicy::Maximum);
		expand->setMaximumSize(10, 16);
		expand->setMinimumSize(10, 0);
		boxLayout->insertWidget(0, expand);

		obs_data_t *data = obs_sceneitem_get_private_settings(sceneitem);
		expand->blockSignals(true);
		expand->setChecked(obs_data_get_bool(data, "collapsed"));
		expand->blockSignals(false);
		obs_data_release(data);

		connect(expand, &QPushButton::toggled,
				this, &SourceTreeItem::ExpandClicked);

	} else {
		int space = tree->GetStm()->hasGroups ? 10 : 3;
		spacer = new QSpacerItem(space, 1);
		boxLayout->insertItem(0, spacer);
	}
}

void SourceTreeItem::ExpandClicked(bool checked)
{
	OBSData data = obs_sceneitem_get_private_settings(sceneitem);
	obs_data_release(data);

	obs_data_set_bool(data, "collapsed", checked);

	if (!checked)
		tree->GetStm()->ExpandGroup(sceneitem);
	else
		tree->GetStm()->CollapseGroup(sceneitem);
}

/* ========================================================================= */

void SourceTreeModel::OBSFrontendEvent(enum obs_frontend_event event, void *ptr)
{
	SourceTreeModel *stm = reinterpret_cast<SourceTreeModel *>(ptr);

	switch ((int)event) {
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
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

	hasGroups = false;
}

static bool enumItem(obs_scene_t*, obs_sceneitem_t *item, void *ptr)
{
	QVector<OBSSceneItem> &items =
		*reinterpret_cast<QVector<OBSSceneItem>*>(ptr);

	obs_sceneitem_group_t *group = obs_sceneitem_group_from_item(item);
	if (group) {
		obs_data_t *data = obs_sceneitem_get_private_settings(item);

		bool collapse = obs_data_get_bool(data, "collapsed");
		if (!collapse) {
			obs_scene_t *scene =
				obs_sceneitem_group_get_scene(group);

			obs_scene_enum_items(scene, enumItem, &items);
		}

		obs_data_release(data);
	}

	items.insert(0, item);
	return true;
}

void SourceTreeModel::SceneChanged()
{
	OBSScene scene = GetCurrentScene();

	beginResetModel();
	items.clear();
	obs_scene_enum_items(scene, enumItem, &items);
	endResetModel();

	UpdateGroupState(false);
	st->ResetWidgets();
}

/* reorders list optimally with model reorder funcs */
void SourceTreeModel::ReorderItems()
{
	OBSScene scene = GetCurrentScene();

	QVector<OBSSceneItem> newitems;
	auto enumItem = [] (obs_scene_t*, obs_sceneitem_t *item, void *ptr)
	{
		QVector<OBSSceneItem> &newitems =
			*reinterpret_cast<QVector<OBSSceneItem>*>(ptr);
		newitems.insert(0, item);
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
			obs_sceneitem_t *oldItem = items[i];
			obs_sceneitem_t *newItem = newitems[i];
			if (oldItem != newItem) {
				idx1Old = i;
				break;
			}
		}

		/* if everything is the same, break */
		if (i == newitems.count()) {
			break;
		}

		/* find new starting index */
		for (i = idx1Old + 1; i < newitems.count(); i++) {
			obs_sceneitem_t *oldItem = items[idx1Old];
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

			obs_sceneitem_t *oldItem = items[oldIdx];
			obs_sceneitem_t *newItem = newitems[newIdx];

			if (oldItem != newItem) {
				break;
			}
		}

		/* move items */
		beginMoveRows(QModelIndex(), idx1Old, idx1Old + count - 1,
		              QModelIndex(), idx1New + count);
		for (i = 0; i < count; i++) {
			int to = idx1New + count;
			if (to > idx1Old)
				to--;
			items.move(idx1Old, to);
		}
		endMoveRows();
	}
}

void SourceTreeModel::Add(obs_sceneitem_t *item)
{
	beginInsertRows(QModelIndex(), 0, 0);
	items.insert(0, item);
	endInsertRows();

	st->UpdateWidget(createIndex(0, 0, nullptr), item);
}

void SourceTreeModel::Remove(obs_sceneitem_t *item)
{
	int idx = -1;
	for (int i = 0; i < items.count(); i++) {
		if (items[i] == item) {
			idx = i;
			break;
		}
	}

	if (idx == -1)
		return;

	obs_sceneitem_group_t *group = obs_sceneitem_group_from_item(item);
	int startIdx = idx;
	int endIdx = idx;

	if (!!group) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(group);

		for (int i = endIdx + 1; i < items.count(); i++) {
			obs_sceneitem_t *subitem = items[i];
			obs_scene_t *subscene =
				obs_sceneitem_get_scene(subitem);

			if (subscene == scene)
				endIdx = i;
			else
				break;
		}
	}

	beginRemoveRows(QModelIndex(), startIdx, endIdx);
	items.remove(idx, endIdx - startIdx + 1);
	endRemoveRows();

	if (!!group)
		UpdateGroupState(true);
}

OBSSceneItem SourceTreeModel::Get(int idx)
{
	if (idx == -1 || idx >= items.count())
		return OBSSceneItem();
	return items[idx];
}

SourceTreeModel::SourceTreeModel(SourceTree *st_)
	: QAbstractListModel (st_),
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
	if (!index.isValid())
		return QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;

	return QAbstractListModel::flags(index) |
	       Qt::ItemIsEditable |
	       Qt::ItemIsDragEnabled /*| TODO: if group, allow "drop in"
	       Qt::ItemIsDropEnabled*/;
}

Qt::DropActions SourceTreeModel::supportedDropActions() const
{
	return QAbstractItemModel::supportedDropActions() | Qt::MoveAction;
}

void SourceTreeModel::GroupSelectedItems(QModelIndexList &indices)
{
	OBSScene scene = GetCurrentScene();
	QString name;

	int i = 1;
	for (;;) {
		name = QTStr("Basic.Main.Group").arg(QString::number(i++));
		obs_sceneitem_group_t *group = obs_scene_get_group(scene,
				QT_TO_UTF8(name));
		if (!group)
			break;
	}

	QVector<obs_sceneitem_t *> item_order;

	for (int i = indices.count() - 1; i >= 0; i--) {
		obs_sceneitem_t *item = items[indices[i].row()];
		item_order << item;
	}

	obs_sceneitem_group_t *group = obs_scene_insert_group(
			scene, QT_TO_UTF8(name),
			item_order.data(), item_order.size());

	obs_sceneitem_t *item = obs_sceneitem_group_get_item(group);

	int newIdx = indices[0].row();

	beginInsertRows(QModelIndex(), newIdx, newIdx);
	items.insert(newIdx, item);
	endInsertRows();

	for (int i = 0; i < indices.size(); i++) {
		int fromIdx = indices[i].row() + 1;
		int toIdx = newIdx + i + 1;
		if (fromIdx != toIdx) {
			beginMoveRows(QModelIndex(), fromIdx, fromIdx,
			              QModelIndex(), toIdx);
			items.move(fromIdx, toIdx);
			endMoveRows();
		}
	}

	hasGroups = true;
	st->UpdateWidgets(true);
}

void SourceTreeModel::ExpandGroup(obs_sceneitem_t *item)
{
	int itemIdx = items.indexOf(item);
	if (itemIdx == -1)
		return;

	itemIdx++;

	obs_sceneitem_group_t *group = obs_sceneitem_group_from_item(item);
	obs_scene_t *scene = obs_sceneitem_group_get_scene(group);

	QVector<OBSSceneItem> subItems;
	obs_scene_enum_items(scene, enumItem, &subItems);

	if (!subItems.size())
		return;

	beginInsertRows(QModelIndex(), itemIdx, itemIdx + subItems.size() - 1);
	for (int i = 0; i < subItems.size(); i++)
		items.insert(i + itemIdx, subItems[i]);
	endInsertRows();

	st->UpdateWidgets();
}

void SourceTreeModel::CollapseGroup(obs_sceneitem_t *item)
{
	int startIdx = -1;
	int endIdx = -1;

	obs_sceneitem_group_t *group = obs_sceneitem_group_from_item(item);
	obs_scene_t *scene = obs_sceneitem_group_get_scene(group);

	for (int i = 0; i < items.size(); i++) {
		obs_scene_t *itemScene = obs_sceneitem_get_scene(items[i]);

		if (itemScene == scene) {
			if (startIdx == -1)
				startIdx = i;
			endIdx = i;
		}
	}

	if (startIdx == -1)
		return;

	beginRemoveRows(QModelIndex(), startIdx, endIdx);
	items.remove(startIdx, endIdx - startIdx + 1);
	endRemoveRows();
}

void SourceTreeModel::UpdateGroupState(bool update)
{
	bool nowHasGroups = false;
	for (auto &item : items) {
		if (!!obs_sceneitem_group_from_item(item)) {
			nowHasGroups = true;
			break;
		}
	}

	if (nowHasGroups != hasGroups) {
		hasGroups = nowHasGroups;
		if (update)
			st->UpdateWidgets(true);
	}
}

/* ========================================================================= */

SourceTree::SourceTree(QWidget *parent_) : QListView(parent_)
{
	SourceTreeModel *stm_ = new SourceTreeModel(this);
	setModel(stm_);
}

void SourceTree::ResetWidgets()
{
	OBSScene scene = GetCurrentScene();

	SourceTreeModel *stm = GetStm();
	stm->UpdateGroupState(false);

	for (int i = 0; i < stm->items.count(); i++) {
		QModelIndex index = stm->createIndex(i, 0, nullptr);
		setIndexWidget(index, new SourceTreeItem(this, stm->items[i]));
	}
}

void SourceTree::UpdateWidget(const QModelIndex &idx, obs_sceneitem_t *item)
{
	setIndexWidget(idx, new SourceTreeItem(this, item));
}

void SourceTree::UpdateWidgets(bool force)
{
	SourceTreeModel *stm = GetStm();

	for (int i = 0; i < stm->items.size(); i++) {
		obs_sceneitem_t *item = stm->items[i];
		SourceTreeItem *widget = GetItemWidget(i);

		if (!widget) {
			UpdateWidget(stm->createIndex(i, 0), item);
		} else {
			widget->Update(force);
		}
	}
}

void SourceTree::SelectItem(obs_sceneitem_t *sceneitem, bool select)
{
	SourceTreeModel *stm = GetStm();
	int i = 0;

	for (; i < stm->items.count(); i++) {
		if (stm->items[i] == sceneitem)
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
	if (event->source() != this) {
		QListView::dropEvent(event);
		return;
	}

	SourceTreeModel *stm = GetStm();
	QModelIndexList indices = selectedIndexes();
	QList<QPersistentModelIndex> persistentIndices;

	persistentIndices.reserve(indices.count());
	for (QModelIndex &index : indices)
		persistentIndices.append(index);
	std::sort(persistentIndices.begin(), persistentIndices.end());

	DropIndicatorPosition indicator = dropIndicatorPosition();
	int row = indexAt(event->pos()).row();

	if (indicator == QAbstractItemView::BelowItem)
		row++;

	if (row >= 0 && row <= stm->items.count()) {
		int r = row;
		for (auto persistentIdx : persistentIndices) {
			int from = persistentIdx.row();
			int to = r;
			int itemTo = to;

			if (itemTo > from)
				itemTo--;

			if (itemTo != from) {
				stm->beginMoveRows(QModelIndex(), from, from,
						  QModelIndex(), to);
				stm->items.move(from, itemTo);
				stm->endMoveRows();
			}

			r = persistentIdx.row() + 1;
		}

		event->accept();
		event->setDropAction(Qt::CopyAction);

		OBSScene scene = GetCurrentScene();

		auto updateScene = [] (void *data, obs_scene_t *scene)
		{
			QVector<OBSSceneItem> &items =
				*reinterpret_cast<QVector<OBSSceneItem>*>(data);

			QVector<obs_sceneitem_t*> ptrList;
			ptrList.reserve(items.size());
			for (OBSSceneItem &item : items)
				ptrList.insert(0, item);

			obs_scene_reorder_items(scene, ptrList.data(),
					ptrList.size());
		};

		ignoreReorder = true;
		obs_scene_atomic_update(scene, updateScene, &stm->items);
		ignoreReorder = false;
	}

	QListView::dropEvent(event);
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
			obs_sceneitem_select(stm->items[idx], true);
		}

		for (int i = 0; i < deselectedIdxs.count(); i++) {
			int idx = deselectedIdxs[i].row();
			obs_sceneitem_select(stm->items[idx], false);
		}
	}
	QListView::selectionChanged(selected, deselected);
}

void SourceTree::Edit(int row)
{
	SourceTreeModel *stm = GetStm();
	if (row < 0 || row >= stm->items.count())
		return;

	QWidget *widget = indexWidget(stm->createIndex(row, 0));
	SourceTreeItem *itemWidget = reinterpret_cast<SourceTreeItem *>(widget);
	itemWidget->EnterEditMode();
	edit(stm->createIndex(row, 0));
}

bool SourceTree::MultipleBaseSelected() const
{
	SourceTreeModel *stm = GetStm();
	QModelIndexList selectedIndices = selectedIndexes();

	OBSScene scene = GetCurrentScene();

	if (selectedIndices.size() < 2) {
		return false;
	}

	for (auto &idx : selectedIndices) {
		obs_sceneitem_t *item = stm->items[idx.row()];
		obs_scene *itemScene = obs_sceneitem_get_scene(item);

		if (itemScene != scene) {
			return false;
		}
	}

	return true;
}

bool SourceTree::GroupedItemsSelected() const
{
	SourceTreeModel *stm = GetStm();
	QModelIndexList selectedIndices = selectedIndexes();
	OBSScene scene = GetCurrentScene();

	if (!selectedIndices.size()) {
		return false;
	}

	for (auto &idx : selectedIndices) {
		obs_sceneitem_t *item = stm->items[idx.row()];
		obs_scene *itemScene = obs_sceneitem_get_scene(item);

		if (itemScene != scene) {
			return true;
		}
	}

	return false;
}

void SourceTree::GroupSelectedItems()
{
	QModelIndexList indices = selectedIndexes();
	std::sort(indices.begin(), indices.end());
	GetStm()->GroupSelectedItems(indices);
}
