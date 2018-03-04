#pragma once

#include <QList>
#include <QVector>
#include <QPointer>
#include <QListView>
#include <QAbstractListModel>

class QLabel;
class QCheckBox;
class QLineEdit;
class SourceTree;
class QSpacerItem;
class QHBoxLayout;
class LockedCheckBox;
class VisibilityCheckBox;
class VisibilityItemWidget;

class SourceTreeItem : public QWidget {
	Q_OBJECT

	friend class SourceTree;
	friend class SourceTreeModel;

	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

	virtual bool eventFilter(QObject *object, QEvent *event) override;

public:
	explicit SourceTreeItem(SourceTree *tree, OBSSceneItem sceneitem);

private:
	QCheckBox *expand = nullptr;
	VisibilityCheckBox *vis = nullptr;
	LockedCheckBox *lock = nullptr;
	QHBoxLayout *boxLayout = nullptr;
	QLabel *label = nullptr;

	QLineEdit *editor = nullptr;

	SourceTree *tree;
	OBSSceneItem sceneitem;
	OBSSignal sceneRemoveSignal;
	OBSSignal itemRemoveSignal;
	OBSSignal visibleSignal;
	OBSSignal renameSignal;

private slots:
	void EnterEditMode();
	void ExitEditMode(bool save);

	void VisibilityChanged(bool visible);
	void Renamed(const QString &name);
};

class SourceTreeModel : public QAbstractListModel {
	Q_OBJECT

	friend class SourceTree;

	SourceTree *st;
	QVector<OBSSceneItem> items;

	static void OBSFrontendEvent(enum obs_frontend_event event, void *ptr);
	void Clear();
	void SceneChanged();
	void ReorderItems();

	void Add(obs_sceneitem_t *item);
	void Remove(obs_sceneitem_t *item);
	OBSSceneItem Get(int idx);

public:
	explicit SourceTreeModel(SourceTree *st);
	~SourceTreeModel();

	virtual int rowCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;

	virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
	virtual Qt::DropActions supportedDropActions() const override;
};

class SourceTree : public QListView {
	Q_OBJECT

	bool ignoreReorder = false;

	friend class SourceTreeModel;

	void ResetWidgets();
	void UpdateWidget(const QModelIndex &idx, obs_sceneitem_t *item);

	inline SourceTreeModel *GetStm() const
	{
		return reinterpret_cast<SourceTreeModel *>(model());
	}

public:
	explicit SourceTree(QWidget *parent = nullptr);

	inline bool IgnoreReorder() const {return ignoreReorder;}
	inline void ReorderItems() {GetStm()->ReorderItems();}
	inline void Clear() {GetStm()->Clear();}

	inline void Add(obs_sceneitem_t *item) {GetStm()->Add(item);}
	inline void Remove(obs_sceneitem_t *item) {GetStm()->Remove(item);}
	inline OBSSceneItem Get(int idx) {return GetStm()->Get(idx);}

	void SelectItem(obs_sceneitem_t *sceneitem, bool select);

	void Edit(int idx);

protected:
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
	virtual void dropEvent(QDropEvent *event) override;

	virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
};
