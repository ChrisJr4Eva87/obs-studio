#pragma once

#include <QList>
#include <QVector>
#include <QPointer>
#include <QListView>
#include <QAbstractListModel>

class QLabel;
class SourceTree;
class QMouseEvent;
class QCheckBox;
class QSpacerItem;
class QHBoxLayout;
class QRubberBand;
class LockedCheckBox;
class VisibilityCheckBox;
class VisibilityItemWidget;

class SourceTreeItem : public QWidget {
	Q_OBJECT

	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

	void paintEvent(QPaintEvent *event) override;

public:
	explicit SourceTreeItem(SourceTree *tree, const QString &text,
			SourceTreeItem *group = nullptr);

	void RecalculateSpacing();

private:
	QSpacerItem *spacer = nullptr;
	QCheckBox *expand = nullptr;
	VisibilityCheckBox *vis = nullptr;
	LockedCheckBox *lock = nullptr;
	QHBoxLayout *boxLayout = nullptr;
	QLabel *label = nullptr;

	//bool selected = false;

	SourceTree *tree;
	SourceTreeItem *group;
};

class SourceTreeModel : public QAbstractListModel {
	Q_OBJECT

	friend class SourceTree;

	SourceTree *st;
	QVector<QPointer<SourceTreeItem>> items;

	static void OBSFrontendEvent(enum obs_frontend_event event, void *ptr);
	void SceneChanged();

public:
	explicit SourceTreeModel(SourceTree *st);
	~SourceTreeModel();

	virtual int rowCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;
};

class SourceTree : public QListView {
	Q_OBJECT

	bool ignoreReorder = false;

	friend class SourceTreeModel;

	void ResetWidgets();

	inline SourceTreeModel *GetStm() const
	{
		return reinterpret_cast<SourceTreeModel *>(model());
	}

public:
	explicit SourceTree();
	void UpdateScene();

	inline bool IgnoreReorder() const {return ignoreReorder;}

	inline void AddItem(const QString &) {}

protected:
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
	virtual void dropEvent(QDropEvent *event) override;

	virtual void commitData(QWidget *editor) override;
	virtual void currentChanged(const QModelIndex &cur,
			const QModelIndex &prev) override;
	virtual void dataChanged(const QModelIndex &tl, const QModelIndex &br,
			const QVector<int> &roles) override;
	virtual void rowsAboutToBeRemoved(const QModelIndex &parent, int start,
			int end) override;
	virtual void rowsInserted(const QModelIndex &parent,
			int start, int end) override;
};
