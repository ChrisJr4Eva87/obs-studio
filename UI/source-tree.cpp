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
#include <QSpacerItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>

#include <QStylePainter>
#include <QStyleOptionFocusRect>

/* ========================================================================= */

SourceTreeItem::SourceTreeItem(SourceTree *tree_, const QString &text,
		SourceTreeItem *group_)
	: tree  (tree_),
	  group (group_)
{
	setAttribute(Qt::WA_TranslucentBackground);

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

	label = new QLabel(text);
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

	SourceTreeItem *item = group;
	while (item) {
		indentation += 16;
		item = item->group;
	}

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
	blog(LOG_DEBUG, "how often is this being called?  %d", chi++);

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
	blog(LOG_DEBUG, "====================================");

	QColor textColor = pal.color(group, textRole);
	QBrush bkColor = pal.brush(group, bkRole);

	pal.setColor(QPalette::WindowText, textColor);

	/* ----------------------------- */
	/* set widget colors             */

	//vis->setPalette(pal);
	//label->setStyleSheet(QString("color: %1;").arg(textColor.name()));

	QWidget::paintEvent(event);
}

/* ========================================================================= */

void SourceTreeModel::OBSFrontendEvent(enum obs_frontend_event event, void *ptr)
{
	SourceTreeModel *stm = reinterpret_cast<SourceTreeModel *>(ptr);

	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		stm->SceneChanged();
		break;
	default:;
	}
}

void SourceTreeModel::SceneChanged()
{
	OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();

	auto addItem = [this] (obs_sceneitem_t *sceneitem)
	{
		obs_source_t *source = obs_sceneitem_get_source(sceneitem);
		const char *name = obs_source_get_name(source);
		SourceTreeItem *item = new SourceTreeItem(st, QT_UTF8(name),
				nullptr);
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

/* ========================================================================= */

SourceTree::SourceTree() : QListView()
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

	/* ... */
}

void SourceTree::commitData(QWidget *editor)
{
	blog(LOG_DEBUG, "SourceTree::commitData");
	QListView::commitData(editor);
}

void SourceTree::currentChanged(const QModelIndex &cur,
		const QModelIndex &prev)
{
	blog(LOG_DEBUG, "SourceTree::currentChanged: (%d, %d), (%d, %d)",
			cur.row(), cur.column(), prev.row(), prev.column());
	QListView::currentChanged(cur, prev);
}

void SourceTree::dataChanged(const QModelIndex &tl, const QModelIndex &br,
		const QVector<int> &roles)
{
	blog(LOG_DEBUG, "SourceTree::dataChanged: (%d, %d) - (%d, %d)",
			tl.row(), tl.column(), br.row(), br.column());
	QListView::dataChanged(tl, br, roles);
}

void SourceTree::rowsAboutToBeRemoved(const QModelIndex &parent, int start,
		int end)
{
	blog(LOG_DEBUG, "SourceTree::rowsAboutToBeRemoved: (%d, %d), %d, %d",
			parent.row(), parent.column(), start, end);
	QListView::rowsAboutToBeRemoved(parent, start, end);
}

void SourceTree::rowsInserted(const QModelIndex &parent,
		int start, int end)
{
	blog(LOG_DEBUG, "SourceTree::rowsInserted: (%d, %d), %d, %d",
			parent.row(), parent.column(), start, end);
	QListView::rowsAboutToBeRemoved(parent, start, end);
}
