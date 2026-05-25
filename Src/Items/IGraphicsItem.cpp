#include "IGraphicsItem.h"
#include "RectItem.h"
#include "EllipseItem.h"
#include "LineItem.h"
#include "BezierCurveItem.h"
#include "TextItem.h"
#include "ImageItem.h"
#include "FreehandItem.h"
#include "GraphicsItemGroup.h"

IGraphicsItem *createItemByType(IGraphicsItem::ItemType type)
{
    switch (type) {
    case IGraphicsItem::RectItemType:       return new RectItem;
    case IGraphicsItem::EllipseItemType:    return new EllipseItem;
    case IGraphicsItem::LineItemType:       return new LineItem;
    case IGraphicsItem::BezierCurveItemType: return new BezierCurveItem;
    case IGraphicsItem::TextItemType:       return new TextItem;
    case IGraphicsItem::ImageItemType:      return new ImageItem;
    case IGraphicsItem::FreehandItemType:   return new FreehandItem;
    case IGraphicsItem::GroupItemType:      return new GraphicsItemGroup;
    }
    return nullptr;
}
