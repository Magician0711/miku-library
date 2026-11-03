#pragma once

#include <QLayout>
#include <QRect>
#include <QStyle>

class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget *parent = nullptr, int hSpacing = 6, int vSpacing = 6);
    ~FlowLayout() override;

    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;

    void clear();

private:
    int doLayout(const QRect &rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem *> m_items;
    int m_hSpace;
    int m_vSpace;
};
