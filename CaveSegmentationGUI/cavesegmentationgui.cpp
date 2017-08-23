#include "stdafx.h"

#include "cavesegmentationgui.h"
#include <qmessagebox.h>
#include <qfiledialog.h>

#include <FileInputOutput.h>
#include <QtConcurrent>

CaveSegmentationGUI::CaveSegmentationGUI(const AppOptions& o, QWidget *parent)
	: QMainWindow(parent)
{
	options = o;

	ui.setupUi(this);	

	glView = new CaveDataGLView(vm, (o.stereo ? -1 : 0), this);
	glView->setVirtualAspectMultiplier(o.aspectMultiplier);
	this->setCentralWidget(glView);

	plot = std::make_unique<DataPlot>(vm, ui.grpData);
	ui.grpData->layout()->addWidget(plot.get());
	plot->setFixedHeight(300);

	ui.chkLookThrough->setChecked(vm.getLookThrough());
	connect(ui.chkLookThrough, &QCheckBox::stateChanged, this, &CaveSegmentationGUI::lookThroughChanged);

	vm.distanceExponent.set(1.0f);
	vm.distanceExponent.setupLabel(ui.lblDistanceExponent);
	vm.distanceExponent.setupSlider(ui.sldDistanceExponent, 0.5f, 3.0f, 0.1f);

	vm.caveScaleKernelFactor.setupLabel(ui.lblCaveScaleKernel);
	vm.caveScaleKernelFactor.setupSlider(ui.sldCaveScaleKernel, 1.0, 20.0, 0.01);

	vm.caveSizeKernelFactor.setupLabel(ui.lblSizeKernel);
	vm.caveSizeKernelFactor.setupSlider(ui.sldSizeKernel, 0.01, 2.0, 0.01);

	vm.caveSizeDerivativeKernelFactor.setupLabel(ui.lblDerivativeKernel);
	vm.caveSizeDerivativeKernelFactor.setupSlider(ui.sldDerivativeKernel, 0.01, 2.0, 0.01);

	vm.tippingCurvature.setupLabel(ui.lblTippingCurvature);
	vm.tippingCurvature.setupSlider(ui.sldTippingCurvature, 0.01, 1.0, 0.01);

	vm.directionTolerance.setupLabel(ui.lblDirectionTolerance);
	vm.directionTolerance.setupSlider(ui.sldDirectionTolerance, 0.01, 2.0, 0.01);

	vm.edgeCollapseThreshold.setupLabel(ui.lblEdgeCollapseThreshold);
	vm.edgeCollapseThreshold.setupSlider(ui.sldEdgeCollapseThreshold, 0.1, 10, 0.1);

	vm.skeletonSmooth.setupLabel(ui.lblSkeletonSmooth);
	vm.skeletonSmooth.setupSlider(ui.sldSkeletonSmooth, 0.0, 40.0, 0.01);

	vm.skeletonVelocity.setupLabel(ui.lblSkeletonVelocity);
	vm.skeletonVelocity.setupSlider(ui.sldSkeletonVelocity, 0.0, 40.0, 0.01);

	vm.skeletonMedial.setupLabel(ui.lblSkeletonMedial);
	vm.skeletonMedial.setupSlider(ui.sldSkeletonMedial, 0.0, 40.0, 0.01);

	connect(&vm.caveData, &CaveGLData::meshChanged, this, &CaveSegmentationGUI::meshChanged); meshChanged();
	connect(&vm.caveData, &CaveGLData::skeletonChanged, this, &CaveSegmentationGUI::skeletonChanged); skeletonChanged();
	connect(&vm.caveData, &CaveGLData::distancesChanged, this, &CaveSegmentationGUI::distancesChanged); distancesChanged();

	connect(&vm.caveData, &CaveGLData::distancesChanged, this, &CaveSegmentationGUI::segmentationParametersChanged);
	connect(&vm.tippingCurvature, &ObservableVariable<double>::changed, this, &CaveSegmentationGUI::segmentationParametersChanged);
	connect(&vm.directionTolerance, &ObservableVariable<double>::changed, this, &CaveSegmentationGUI::segmentationParametersChanged);

	connect(ui.btnLoadOff, &QPushButton::clicked, this, &CaveSegmentationGUI::loadOff);	
	connect(ui.btnLoadSkeleton, &QPushButton::clicked, this, &CaveSegmentationGUI::loadSkeleton);
	connect(ui.btnCalculateSkeleton, &QPushButton::clicked, this, &CaveSegmentationGUI::calculateSkeleton);
	connect(ui.btnAbortSkeleton, &QPushButton::clicked, this, &CaveSegmentationGUI::abortSkeleton);
	connect(ui.btnSaveSkeleton, &QPushButton::clicked, this, &CaveSegmentationGUI::saveSkeleton);
	connect(ui.btnLoadDistances, &QPushButton::clicked, this, &CaveSegmentationGUI::loadDistances);
	connect(ui.btnCalculateDistances, &QPushButton::clicked, this, &CaveSegmentationGUI::calculateDistances);
	connect(ui.btnCalculateSpecificVertexDistance, &QPushButton::clicked, this, &CaveSegmentationGUI::calculateSpecificVertexDistances);
	connect(ui.btnSaveDistances, &QPushButton::clicked, this, &CaveSegmentationGUI::saveDistances);
	connect(ui.btnRunSegmentation, &QPushButton::clicked, this, &CaveSegmentationGUI::runSegmentation);
	connect(ui.btnLoadSegmentation, &QPushButton::clicked, this, &CaveSegmentationGUI::loadSegmentation);
	connect(ui.btnResetDefaults, &QPushButton::clicked, this, &CaveSegmentationGUI::resetToDefaults);	
	connect(ui.btnSaveSegmentation, &QPushButton::clicked, this, &CaveSegmentationGUI::saveSegmentation);
	connect(ui.btnSaveSegmentedMesh, &QPushButton::clicked, this, &CaveSegmentationGUI::saveSegmentedMesh);

	connect(&skeletonWatcher, &QFutureWatcher<CurveSkeleton*>::finished, this, &CaveSegmentationGUI::skeletonComputationFinished);

	if (!o.stereo)
	{
		setWindowState(windowState() | Qt::WindowMaximized);
		connect(glView, &GLView::inited, this, &CaveSegmentationGUI::preloadData);
	}
	else
	{
		secondary = std::make_unique<Secondary>(ui.dockWidget, this);
		secondary->setWindowTitle(this->windowTitle());
		secondary->show();

		secondarySynchronizationTimer.setInterval(40);
		connect(&secondarySynchronizationTimer, &QTimer::timeout, this, &CaveSegmentationGUI::synchronizeSecondary);
		secondarySynchronizationTimer.start();

		connect(glView, &GLView::inited, this, &CaveSegmentationGUI::glViewInited);

		QRect rec = QApplication::desktop()->availableGeometry(o.screenNumber);
		setGeometry(rec.x(), rec.y(), rec.width() / 2, rec.height());
	}	
}

void CaveSegmentationGUI::glViewInited()
{
	if (options.stereo)
	{
		secondaryGlView = std::make_unique<CaveDataGLView>(vm, 1, secondary.get(), glView);
		secondaryGlView->setVirtualAspectMultiplier(options.aspectMultiplier);
		secondary->setCentralWidget(secondaryGlView.get());

		connect(secondaryGlView.get(), &GLView::inited, this, &CaveSegmentationGUI::preloadData);
	}	
}

void CaveSegmentationGUI::preloadData()
{
	this->setFixedWidth(1920 + width() - glView->width());
	this->setFixedHeight(1080 + height() - glView->height());
	if (!options.dataDir.isEmpty())
	{
		dataDirectory = QDir(options.dataDir);

		QString modelPath = dataDirectory.absoluteFilePath("model.off");

		if (QFile(modelPath).exists())
			vm.caveData.LoadMesh(modelPath.toStdString());
		else
			return;

		QString skeletonPath = dataDirectory.absoluteFilePath("model.skel");
		if (QFile(modelPath).exists())
			vm.caveData.SetSkeleton(LoadCurveSkeleton(skeletonPath.toStdString().c_str()));
		else
			return;

		QString distancesPath = dataDirectory.absoluteFilePath("distances.bin");
		if (QFile(distancesPath).exists())
		{
			vm.caveData.LoadDistances(distancesPath.toStdString());
			vm.caveData.SmoothAndDeriveDistances();
		}
		else
			return;
	}
}

void CaveSegmentationGUI::synchronizeSecondary()
{
	secondary->setGeometry(QRect(this->geometry().x() + QApplication::desktop()->availableGeometry(this).width() / 2, this->geometry().y(), this->geometry().width(), this->geometry().height()));

	secondary->CopyFromPrimary();

	if(secondaryGlView)
		secondaryGlView->setGeometry(glView->geometry());
}

CaveSegmentationGUI::~CaveSegmentationGUI()
{
}

void CaveSegmentationGUI::meshChanged()
{
	ui.grpSkeleton->setEnabled(vm.caveData.meshVertices().size() > 0);
}

void CaveSegmentationGUI::skeletonChanged()
{
	ui.grpData->setEnabled(vm.caveData.skeleton != nullptr);
}

void CaveSegmentationGUI::distancesChanged()
{
	ui.grpSegmentation->setEnabled(vm.caveData.caveSizes.size() > 0);
}


void CaveSegmentationGUI::loadOff(bool)
{
	auto filename = QFileDialog::getOpenFileName(this, "Load OFF", options.initialDirectory, "3D Models (*.off)");
	if (!filename.isEmpty())
	{
		modelFilename = filename.toStdString();
		vm.selectedPath.clear();
		emit vm.selectedPathChanged();
		vm.caveData.LoadMesh(filename.toStdString());
		dataDirectory = QDir(filename);
		dataDirectory.cdUp();
	}
}

QString CaveSegmentationGUI::getFilepath(QString defaultFile, QString title, QString filter)
{
	QString filename;
	if (dataDirectory.exists() && dataDirectory.exists(defaultFile))
	{
		filename = dataDirectory.absoluteFilePath(defaultFile);
	}

	if (filename.isEmpty())
		filename = QFileDialog::getOpenFileName(this, title, QString(), filter);
	return filename;
}

void CaveSegmentationGUI::loadSkeleton(bool)
{
	QString filename = getFilepath("model.skel", "Load Skeleton", "Skeleton (*.skel)");

	if (!filename.isEmpty())
	{
		CurveSkeleton* skeleton = LoadCurveSkeleton(filename.toStdString().c_str());
		vm.caveData.SetSkeleton(skeleton);
	}
}

void CaveSegmentationGUI::calculateSkeleton(bool)
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
	float edgeCollapseThreshold = (vm.caveData.getMax() - vm.caveData.getMin()).norm() * vm.edgeCollapseThreshold.get() / 100.0f;
	skeletonComputation = QtConcurrent::run(std::bind(ComputeCurveSkeleton, modelFilename, &skeletonAbort, edgeCollapseThreshold, vm.skeletonSmooth.get(), vm.skeletonVelocity.get(), vm.skeletonMedial.get()));
	skeletonWatcher.setFuture(skeletonComputation);
	ui.btnCalculateSkeleton->setEnabled(false);
}

void CaveSegmentationGUI::skeletonComputationFinished()
{
	auto skeleton = skeletonComputation.result();
	if (skeleton)
		vm.caveData.SetSkeleton(skeleton);
	QApplication::restoreOverrideCursor();
	ui.btnCalculateSkeleton->setEnabled(true);
}

void CaveSegmentationGUI::abortSkeleton(bool)
{
	skeletonAbort.abort = true;
}

void CaveSegmentationGUI::saveSkeleton(bool)
{
	QString filename = QFileDialog::getSaveFileName(this, "Save Skeleton", QString(), "Skeleton File (*.skel)");
	if (!filename.isEmpty())
	{
		vm.caveData.skeleton->Save(filename.toStdString().c_str());
	}
}

void CaveSegmentationGUI::loadDistances(bool)
{
	QString filename = getFilepath("distances.bin", "Load Distances", "Binary File (*.bin)");

	if (!filename.isEmpty())
	{
		vm.caveData.LoadDistances(filename.toStdString());
		vm.caveData.SmoothAndDeriveDistances();
	}
}

void CaveSegmentationGUI::calculateDistances(bool)
{
	if (!vm.caveData.CalculateDistances(vm.distanceExponent.get()))
	{		
#pragma omp parallel for
		for (int i = 0; i < vm.caveData.skeleton->vertices.size(); ++i)
			vm.caveData.colorLayer.at(i) = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

		for(int i : vm.caveData.invalidVertices)			
			vm.caveData.colorLayer.at(i) = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
				
		vm.caveData.UpdateColorLayer();
		QMessageBox::critical(this, "Calculate Distances", QStringLiteral("There are %1 vertices with invalid distances. Make sure that the skeleton is valid and is completely contained within the cave.").arg(vm.caveData.invalidVertices.size()));
	}
	vm.caveData.SmoothAndDeriveDistances();
}

void CaveSegmentationGUI::calculateSpecificVertexDistances(bool)
{
	if (vm.selectedVertex.get() < 0)
		return;
	
	vm.caveData.CalculateDistancesSingleVertex<SphereVisualizer>(vm.selectedVertex.get(), vm.distanceExponent.get());
	QMessageBox::information(this, "Distance Calculation", "Calculated Cave Size at vertex " + QString::number(vm.selectedVertex.get()) + ": " + QString::number(vm.caveData.caveSizeUnsmoothed.at(vm.selectedVertex.get())));
}

void CaveSegmentationGUI::saveDistances(bool)
{
	QString filename = QFileDialog::getSaveFileName(this, "Save Distances", QString(), "Binary File (*.bin)");
	if (!filename.isEmpty())
	{
		vm.caveData.SaveDistances(filename.toStdString().c_str());
	}
}

void CaveSegmentationGUI::runSegmentation(bool)
{
	vm.caveData.RunSegmentation();
}

void CaveSegmentationGUI::loadSegmentation(bool)
{
	QString filename = QFileDialog::getOpenFileName(this, "Load Segmentation", QString(), "Segmentation file (*.seg)");
	if (!filename.isEmpty())
	{
		vm.caveData.LoadSegmentation(filename.toStdString());
	}
}

void CaveSegmentationGUI::lookThroughChanged(int state)
{
	vm.setLookThrough(state == Qt::Checked);
}

void CaveSegmentationGUI::segmentationParametersChanged()
{
	if (ui.chkUpdateSegmentation->isChecked() && vm.caveData.skeleton && vm.caveData.caveSizes.size() > 0 && vm.caveData.caveSizes.at(0) > 0)
	{
		vm.caveData.RunSegmentation();
	}
}

void CaveSegmentationGUI::resetToDefaults()
{
	vm.caveScaleKernelFactor.set(10.0);
	vm.caveSizeKernelFactor.set(0.2);
	vm.caveSizeDerivativeKernelFactor.set(0.2);
	vm.tippingCurvature.set(0.3);
	vm.directionTolerance.set(0.1);
}

void CaveSegmentationGUI::saveSegmentation()
{
	QString filename = QFileDialog::getSaveFileName(this, "Save Segmentation", QString(), "Segmentation file (*.seg)");
	if (!filename.isEmpty())
	{
		WriteSegmentation(filename.toStdString(), vm.caveData.segmentation);
	}
}

void CaveSegmentationGUI::saveSegmentedMesh()
{
	QString filename = QFileDialog::getSaveFileName(this, "Save Segmented Mesh", QString(), "3D Models (*.off)");
	if (!filename.isEmpty())
	{
		vm.caveData.WriteSegmentationColoredOff(filename.toStdString(), vm.caveData.segmentation);
	}
}