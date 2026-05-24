#ifndef MERGETIFFPROCESSOR_H
#define MERGETIFFPROCESSOR_H

#include "ImageWorker.h"

class MergeTiffProcessor : public ImageUtils::IImportPostProcessor
{
public:
    MergeTiffProcessor();

public:
    virtual QString name() const override;
    // 工作线程中调用，对加载完成的 ImportResult 做后处理（色彩校正、元数据提取等）
    // virtual void process(ImageUtils::ImportResult &result) override;

};

#endif // MERGETIFFPROCESSOR_H
