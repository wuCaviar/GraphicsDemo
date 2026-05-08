#include <QtTest>

#include "AlignmentUtils.h"
#include "IGraphicsItem.h"

#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>

// ============================================================
// 测试 AlignmentUtils 算法正确性
// 使用 QGraphicsRectItem + Qt::NoPen 确保 boundingRect() == rect()，
// 从而可以在不链接 IGraphicsItem.cpp（它依赖所有 Item 子类）的情况下
// 验证算法逻辑。
// ============================================================
class TestAlignment : public QObject
{
    Q_OBJECT

private slots:
    // ---- 对齐测试 ----
    void testAlignLeft();
    void testAlignRight();
    void testAlignTop();
    void testAlignBottom();
    void testAlignHCenter();
    void testAlignVCenter();

    // ---- 自动分布测试 ----
    void testDistributeHAuto();
    void testDistributeVAuto();

    // ---- 自定义间距分布测试 ----
    void testDistributeHCustomSpacing();
    void testDistributeVCustomSpacing();

    // ---- 边界情况 ----
    void testDistributeCustomSpacingZero();
    void testAlignLessThan2Items();
    void testDistributeSingleItem();

    // ---- 精度测试（boundingRect vs geometryRect）----
    void testItemGeometryRectWithPen();

private:
    // 创建指定位置和尺寸的矩形图元（无画笔，避免边距干扰）
    QGraphicsRectItem *createRect(qreal x, qreal y, qreal w, qreal h)
    {
        auto *item = new QGraphicsRectItem(0, 0, w, h);
        item->setPen(Qt::NoPen);
        item->setPos(x, y);
        return item;
    }
};

// ============================================================
// 对齐测试
// ============================================================
void TestAlignment::testAlignLeft()
{
    // 3个图元：左边缘分别在 10, 30, 50
    auto *a = createRect(10, 20, 40, 30);
    auto *b = createRect(30, 40, 40, 30);
    auto *c = createRect(50, 60, 40, 30);

    QList<QGraphicsItem *> items = {a, b, c};
    auto result = AlignmentUtils::computeAlign(items, AlignmentUtils::AlignLeft);

    QVERIFY(result.valid);
    QCOMPARE(result.newPositions.size(), 3);
    // 所有图元左边缘应对齐到 10
    QCOMPARE(result.newPositions[0].x(), 10.0); // a 已在 10
    QCOMPARE(result.newPositions[1].x(), 10.0); // b 从 30 → 10
    QCOMPARE(result.newPositions[2].x(), 10.0); // c 从 50 → 10
    // y 不变
    QCOMPARE(result.newPositions[0].y(), 20.0);
    QCOMPARE(result.newPositions[1].y(), 40.0);
    QCOMPARE(result.newPositions[2].y(), 60.0);

    delete a; delete b; delete c;
}

void TestAlignment::testAlignRight()
{
    auto *a = createRect(10, 20, 40, 30); // 右边缘 50
    auto *b = createRect(30, 40, 40, 30); // 右边缘 70
    auto *c = createRect(50, 60, 40, 30); // 右边缘 90

    QList<QGraphicsItem *> items = {a, b, c};
    auto result = AlignmentUtils::computeAlign(items, AlignmentUtils::AlignRight);

    QVERIFY(result.valid);
    // 所有图元右边缘应对齐到 90
    // a: x + 40 = 90 → x = 50
    QCOMPARE(result.newPositions[0].x(), 50.0);
    // b: x + 40 = 90 → x = 50
    QCOMPARE(result.newPositions[1].x(), 50.0);
    // c: x + 40 = 90 → x = 50
    QCOMPARE(result.newPositions[2].x(), 50.0);

    delete a; delete b; delete c;
}

void TestAlignment::testAlignTop()
{
    auto *a = createRect(10, 10, 40, 30);
    auto *b = createRect(30, 30, 40, 30);
    auto *c = createRect(50, 50, 40, 30);

    QList<QGraphicsItem *> items = {a, b, c};
    auto result = AlignmentUtils::computeAlign(items, AlignmentUtils::AlignTop);

    QVERIFY(result.valid);
    QCOMPARE(result.newPositions[0].y(), 10.0);
    QCOMPARE(result.newPositions[1].y(), 10.0);
    QCOMPARE(result.newPositions[2].y(), 10.0);
    // x 不变
    QCOMPARE(result.newPositions[0].x(), 10.0);
    QCOMPARE(result.newPositions[1].x(), 30.0);
    QCOMPARE(result.newPositions[2].x(), 50.0);

    delete a; delete b; delete c;
}

void TestAlignment::testAlignBottom()
{
    auto *a = createRect(10, 10, 40, 30); // 底边 40
    auto *b = createRect(30, 30, 40, 30); // 底边 60
    auto *c = createRect(50, 50, 40, 30); // 底边 80

    QList<QGraphicsItem *> items = {a, b, c};
    auto result = AlignmentUtils::computeAlign(items, AlignmentUtils::AlignBottom);

    QVERIFY(result.valid);
    // 所有图元底边应对齐到 80
    // a: y + 30 = 80 → y = 50
    QCOMPARE(result.newPositions[0].y(), 50.0);
    QCOMPARE(result.newPositions[1].y(), 50.0);
    QCOMPARE(result.newPositions[2].y(), 50.0);

    delete a; delete b; delete c;
}

void TestAlignment::testAlignHCenter()
{
    // a: 左10 右50, b: 左30 右70, c: 左50 右90
    // 总范围: 左10 右90, 中心 = 50
    auto *a = createRect(10, 20, 40, 30);
    auto *b = createRect(30, 40, 40, 30);
    auto *c = createRect(50, 60, 40, 30);

    QList<QGraphicsItem *> items = {a, b, c};
    auto result = AlignmentUtils::computeAlign(items, AlignmentUtils::AlignHCenter);

    QVERIFY(result.valid);
    // 中心 = 50, 各图元中心也应为 50
    // a: center = 10 + 20 = 30 → x += 20 → x = 30
    QCOMPARE(result.newPositions[0].x(), 30.0);
    // b: center = 30 + 20 = 50 → x 不变
    QCOMPARE(result.newPositions[1].x(), 30.0); // wait, b center was 50, so x stays 30
    // c: center = 50 + 20 = 70 → x -= 20 → x = 30
    QCOMPARE(result.newPositions[2].x(), 30.0);

    delete a; delete b; delete c;
}

void TestAlignment::testAlignVCenter()
{
    auto *a = createRect(10, 10, 40, 30); // 中心 y = 25
    auto *b = createRect(30, 30, 40, 30); // 中心 y = 45
    auto *c = createRect(50, 50, 40, 30); // 中心 y = 65

    QList<QGraphicsItem *> items = {a, b, c};
    auto result = AlignmentUtils::computeAlign(items, AlignmentUtils::AlignVCenter);

    QVERIFY(result.valid);
    // 总范围: top=10 bottom=80, 中心 = 45
    // a: center = 25, offset = 45-25 = 20, y = 30
    QCOMPARE(result.newPositions[0].y(), 30.0);
    // b: center = 45, offset = 0, y = 30... wait
    // b's y = 30, center = 30 + 15 = 45. Target = 45. No change.
    QCOMPARE(result.newPositions[1].y(), 30.0);
    // c: center = 65, offset = 45-65 = -20, y = 30
    QCOMPARE(result.newPositions[2].y(), 30.0);

    delete a; delete b; delete c;
}

// ============================================================
// 自动分布测试
// ============================================================
void TestAlignment::testDistributeHAuto()
{
    // 3个图元各宽50，位于 x=0, 100, 200
    // 范围: 0~250, 总宽 = 150, gap = (250 - 150) / 2 = 50
    auto *a = createRect(0, 0, 50, 30);
    auto *b = createRect(100, 0, 50, 30);
    auto *c = createRect(200, 0, 50, 30);

    QList<QGraphicsItem *> items = {a, b, c};
    AlignmentUtils::DistributeParams params;
    params.autoSpacing = true;

    auto result = AlignmentUtils::computeDistribute(items, AlignmentUtils::DistributeH, params);

    QVERIFY(result.valid);
    QCOMPARE(result.newPositions.size(), 3);

    // a: left=0, right=50
    QCOMPARE(result.newPositions[0].x(), 0.0);
    // b: left=0+50+50=100, right=150
    QCOMPARE(result.newPositions[1].x(), 100.0);
    // c: left=100+50+50=200, right=250
    QCOMPARE(result.newPositions[2].x(), 200.0);

    delete a; delete b; delete c;
}

void TestAlignment::testDistributeVAuto()
{
    // 3个图元各高40，位于 y=0, 200, 400
    // 范围: 0~440, 总高 = 120, gap = (440-120)/2 = 160
    auto *a = createRect(0, 0, 50, 40);
    auto *b = createRect(0, 200, 50, 40);
    auto *c = createRect(0, 400, 50, 40);

    QList<QGraphicsItem *> items = {a, b, c};
    AlignmentUtils::DistributeParams params;
    params.autoSpacing = true;

    auto result = AlignmentUtils::computeDistribute(items, AlignmentUtils::DistributeV, params);

    QVERIFY(result.valid);
    QCOMPARE(result.newPositions[0].y(), 0.0);
    QCOMPARE(result.newPositions[1].y(), 200.0);
    QCOMPARE(result.newPositions[2].y(), 400.0);

    delete a; delete b; delete c;
}

// ============================================================
// 自定义间距分布测试
// ============================================================
void TestAlignment::testDistributeHCustomSpacing()
{
    // 3个图元各宽50，首图元位于 x=0
    // 自定义间距 20px
    // 预期: a at x=0, b at x=0+50+20=70, c at x=70+50+20=140
    auto *a = createRect(0, 0, 50, 30);
    auto *b = createRect(200, 0, 50, 30);
    auto *c = createRect(400, 0, 50, 30);

    QList<QGraphicsItem *> items = {a, b, c};
    AlignmentUtils::DistributeParams params;
    params.autoSpacing = false;
    params.customSpacing = 20.0;

    auto result = AlignmentUtils::computeDistribute(items, AlignmentUtils::DistributeH, params);

    QVERIFY(result.valid);
    QCOMPARE(result.newPositions[0].x(), 0.0);
    QCOMPARE(result.newPositions[1].x(), 70.0);
    QCOMPARE(result.newPositions[2].x(), 140.0);

    delete a; delete b; delete c;
}

void TestAlignment::testDistributeVCustomSpacing()
{
    // 3个图元各高40，首图元位于 y=10
    // 自定义间距 15px
    // 预期: a at y=10, b at y=10+40+15=65, c at y=65+40+15=120
    auto *a = createRect(0, 10, 50, 40);
    auto *b = createRect(0, 300, 50, 40);
    auto *c = createRect(0, 500, 50, 40);

    QList<QGraphicsItem *> items = {a, b, c};
    AlignmentUtils::DistributeParams params;
    params.autoSpacing = false;
    params.customSpacing = 15.0;

    auto result = AlignmentUtils::computeDistribute(items, AlignmentUtils::DistributeV, params);

    QVERIFY(result.valid);
    QCOMPARE(result.newPositions[0].y(), 10.0);
    QCOMPARE(result.newPositions[1].y(), 65.0);
    QCOMPARE(result.newPositions[2].y(), 120.0);

    delete a; delete b; delete c;
}

// ============================================================
// 边界情况
// ============================================================
void TestAlignment::testDistributeCustomSpacingZero()
{
    // 手动模式 + 间距为0 → 紧密排列（无间隙）
    auto *a = createRect(0, 0, 50, 30);
    auto *b = createRect(200, 0, 50, 30);
    auto *c = createRect(400, 0, 50, 30);

    QList<QGraphicsItem *> items = {a, b, c};
    AlignmentUtils::DistributeParams params;
    params.autoSpacing = false;
    params.customSpacing = 0.0;

    auto result = AlignmentUtils::computeDistribute(items, AlignmentUtils::DistributeH, params);

    QVERIFY(result.valid);
    QCOMPARE(result.newPositions[0].x(), 0.0);
    QCOMPARE(result.newPositions[1].x(), 50.0);   // 0 + 50 + 0
    QCOMPARE(result.newPositions[2].x(), 100.0);  // 50 + 50 + 0

    delete a; delete b; delete c;
}

void TestAlignment::testAlignLessThan2Items()
{
    auto *a = createRect(10, 20, 40, 30);
    QList<QGraphicsItem *> items = {a};
    auto result = AlignmentUtils::computeAlign(items, AlignmentUtils::AlignLeft);
    QVERIFY(!result.valid);

    delete a;
}

void TestAlignment::testDistributeSingleItem()
{
    auto *a = createRect(10, 20, 40, 30);
    QList<QGraphicsItem *> items = {a};
    auto result = AlignmentUtils::computeDistribute(items, AlignmentUtils::DistributeH);
    QVERIFY(!result.valid);

    delete a;
}

// ============================================================
// 精度测试：验证 itemGeometryRect 对有画笔图元的正确处理
// ============================================================
void TestAlignment::testItemGeometryRectWithPen()
{
    // QGraphicsRectItem 有 1px 画笔时，boundingRect() 比 rect() 大 1px
    auto *item = new QGraphicsRectItem(0, 0, 50, 50);
    item->setPen(QPen(Qt::black, 1.0));

    // boundingRect() 应包含画笔边距
    QRectF br = item->boundingRect();
    QVERIFY(br.width() > 50.0);   // 应为 51
    QVERIFY(br.height() > 50.0);  // 应为 51

    // 由于 QGraphicsRectItem 不实现 IGraphicsItem::geometryRect()，
    // AlignmentUtils::itemGeometryRect 应 fallback 到 boundingRect()
    QRectF gr = AlignmentUtils::itemGeometryRect(item);
    QCOMPARE(gr, br);

    // 但如果用 NoPen，boundingRect() == rect()
    item->setPen(Qt::NoPen);
    QCOMPARE(item->boundingRect().width(), 50.0);
    QCOMPARE(item->boundingRect().height(), 50.0);

    delete item;
}

QTEST_MAIN(TestAlignment)
#include "tst_alignment.moc"
