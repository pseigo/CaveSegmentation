#ifndef CAVESEGMENTATIONGUICORE_GLOBAL_H
#define CAVESEGMENTATIONGUICORE_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef CAVESEGMENTATIONGUICORE_LIB
# define CAVESEGMENTATIONGUICORE_EXPORT Q_DECL_EXPORT
#else
# define CAVESEGMENTATIONGUICORE_EXPORT Q_DECL_IMPORT
#endif

#endif // CAVESEGMENTATIONGUICORE_GLOBAL_H
