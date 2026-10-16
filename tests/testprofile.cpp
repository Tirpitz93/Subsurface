// SPDX-License-Identifier: GPL-2.0
#include "testprofile.h"
#include "core/device.h"
#include "core/dive.h"
#include "core/divelog.h"
#include "core/divesite.h"
#include "core/trip.h"
#include "core/file.h"
#include "core/save-profiledata.h"
#include "core/pref.h"
#include "core/divecomputer.h"
#include "profile-widget/divecartesianaxis.h"
#include "profile-widget/diveeventitem.h"
#include "profile-widget/diveprofileitem.h"
#include "profile-widget/profilescene.h"
#include "qt-models/diveplannermodel.h"
#include "commands/command.h"
#ifdef MAP_SUPPORT
#include "desktop-widgets/mapwidget.h"
#include "desktop-widgets/mainwindow.h"
#endif
#include "QTextCodec"
#include <QStyleOptionGraphicsItem>
#include <algorithm>

static double depthAccessor(const plot_data &item)
{
	return item.depth.mm;
}

static plot_info createFlatProfile(int points, int stepSeconds, int depthMm)
{
	plot_info pi;
	pi.entry.reserve(points);
	for (int i = 0; i < points; ++i) {
		plot_data data;
		data.sec = i * stepSeconds;
		data.depth.mm = depthMm;
		data.velocity = STABLE;
		pi.entry.push_back(data);
	}
	pi.nr = static_cast<int>(pi.entry.size());
	pi.maxdepth = depthMm;
	pi.maxtime = pi.entry.back().sec;
	return pi;
}

static int countOpaquePixels(const QImage &image, int y)
{
	int count = 0;
	const QRgb *pixel = reinterpret_cast<const QRgb *>(image.constScanLine(y));
	const QRgb *end = pixel + image.width();
	for (; pixel != end; ++pixel) {
		if (qAlpha(*pixel) > 0)
			++count;
	}
	return count;
}

static int longestGapBetweenPaintedPixels(const QImage &image, int y)
{
	const QRgb *pixel = reinterpret_cast<const QRgb *>(image.constScanLine(y));
	const int width = image.width();

	int first = -1;
	int last = -1;
	for (int x = 0; x < width; ++x) {
		if (qAlpha(pixel[x]) > 0) {
			if (first < 0)
				first = x;
			last = x;
		}
	}
	if (first < 0 || last <= first)
		return 0;

	int maxGap = 0;
	int gap = 0;
	for (int x = first; x <= last; ++x) {
		if (qAlpha(pixel[x]) == 0) {
			++gap;
			maxGap = std::max(maxGap, gap);
		} else {
			gap = 0;
		}
	}
	return maxGap;
}

static int longestGapInBand(const QImage &image, int y, int halfBand)
{
	int maxGap = 0;
	for (int yy = std::max(0, y - halfBand); yy <= std::min(image.height() - 1, y + halfBand); ++yy)
		maxGap = std::max(maxGap, longestGapBetweenPaintedPixels(image, yy));
	return maxGap;
}

static int renderStrokePixelsAtY(AbstractProfilePolygonItem &item, int width, int height, int y)
{
	QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);

	QStyleOptionGraphicsItem option;
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing, false);
	item.paint(&painter, &option, nullptr);
	painter.end();

	return countOpaquePixels(image, y);
}

static QImage renderStrokeImage(AbstractProfilePolygonItem &item, int width, int height)
{
	QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);

	QStyleOptionGraphicsItem option;
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing, false);
	item.paint(&painter, &option, nullptr);
	painter.end();

	return image;
}

static std::unique_ptr<dive> createOcDiveWithSamples(int sampleStepSeconds, int gasChangeAtSecond)
{
	auto d = std::make_unique<dive>();

	cylinder_t *bottom = d->get_or_create_cylinder(0);
	bottom->gasmix = gasmix_air;
	bottom->cylinder_use = OC_GAS;

	cylinder_t *deco = d->get_or_create_cylinder(1);
	deco->gasmix.o2.permille = 500;
	deco->gasmix.he.permille = 0;
	deco->cylinder_use = OC_GAS;

	reset_cylinders(d.get(), true);

	divecomputer &dc = d->dcs[0];
	dc.divemode = OC;
	for (int t = 0; t <= 120; t += sampleStepSeconds) {
		sample s;
		s.time.seconds = t;
		s.depth.mm = 12000;
		append_sample(s, &dc);
	}
	dc.duration.seconds = 120;
	add_gas_switch_event(d.get(), &dc, gasChangeAtSecond, 1);

	return d;
}

// This test compares the content of struct profile against a known reference version for a list
// of dives to prevent accidental regressions. Thus is you change anything in the profile this
// test will fail. If this change was intentional, run the test manually. Make sure only the
// indended fields change (for example by computing a diff between exportprofile.csv and
// ..dives/exportprofilereference.csv) and copy the former over the later and commit that change
// as well.

void TestProfile::init()
{
	setlocale(LC_ALL, "C");

	// Set UTF8 text codec as in real applications
	QTextCodec::setCodecForLocale(QTextCodec::codecForMib(106));

	// first, setup the preferences
	prefs = default_prefs;

	QCoreApplication::setOrganizationName("Subsurface");
	QCoreApplication::setOrganizationDomain("subsurface.hohndel.org");
	QCoreApplication::setApplicationName("Subsurface");
}

void TestProfile::cleanup()
{
	DivePlannerPointsModel::instance()->resetPlanState();
	clear_dive_file_data();
}
void TestProfile::testProfileExport()
{
	prefs.planner_deco_mode = BUEHLMANN;
	parse_file(SUBSURFACE_TEST_DATA "/dives/abitofeverything.ssrf", &divelog);
	save_profiledata("exportprofile.csv", false);
	QFile org(SUBSURFACE_TEST_DATA "/dives/exportprofilereference.csv");
	QCOMPARE(org.open(QFile::ReadOnly), true);
	QFile out("exportprofile.csv");
	QCOMPARE(out.open(QFile::ReadOnly), true);
	QTextStream orgS(&org);
	QTextStream outS(&out);
	QString readin = orgS.readAll();
	QString written = outS.readAll();
	QCOMPARE(readin, written);

}
void TestProfile::testProfileExportVPMB()
{
	prefs.planner_deco_mode = VPMB;
	parse_file(SUBSURFACE_TEST_DATA "/dives/abitofeverything.ssrf", &divelog);
	save_profiledata("exportprofileVPMB.csv", false);
	QFile org(SUBSURFACE_TEST_DATA "/dives/exportprofilereferenceVPMB.csv");
	QCOMPARE(org.open(QFile::ReadOnly), true);
	QFile out("exportprofileVPMB.csv");
	QCOMPARE(out.open(QFile::ReadOnly), true);
	QTextStream orgS(&org);
	QTextStream outS(&out);
	QString readin = orgS.readAll();
	QString written = outS.readAll();
	QCOMPARE(readin, written);

}

void TestProfile::testBailoutProfileUsesDashedLineOnHorizontalSegments()
{
	plot_info pi = createFlatProfile(5, 20, 12000);

	ProfileScene axisScene(1.0, false, false);
	DiveCartesianAxis depthAxis(DiveCartesianAxis::Position::Left, true, 3, 0, TIME_GRID, Qt::black, false, false,
				    1.0, 1.0, false, false, axisScene);
	DiveCartesianAxis timeAxis(DiveCartesianAxis::Position::Bottom, false, 2, 0, TIME_GRID, Qt::black, false, false,
				   1.0, 1.0, false, false, axisScene);

	QRectF plotRect(10.0, 10.0, 220.0, 120.0);
	depthAxis.setPosition(plotRect);
	timeAxis.setPosition(plotRect);
	depthAxis.setBounds(0.0, 20000.0);
	timeAxis.setBounds(0.0, 80.0);

	DiveProfileItem primaryItem(pi, timeAxis, depthAxis, &depthAccessor, 1.0);
	primaryItem.replot(nullptr, 0, pi.nr, false);
	primaryItem.setBrush(Qt::NoBrush);

	DiveBailoutProfileItem bailoutItem(pi, timeAxis, depthAxis, &depthAccessor, 1.0);
	bailoutItem.replot(nullptr, 0, pi.nr, false);
	bailoutItem.setBrush(Qt::NoBrush);

	int y = static_cast<int>(std::round(depthAxis.posAtValue(12000.0)));
	QImage primaryImage = renderStrokeImage(primaryItem, 240, 140);
	QImage bailoutImage = renderStrokeImage(bailoutItem, 240, 140);
	int primaryPixels = countOpaquePixels(primaryImage, y);
	int bailoutPixels = countOpaquePixels(bailoutImage, y);
	int primaryMaxGap = longestGapInBand(primaryImage, y, 2);
	int bailoutMaxGap = longestGapInBand(bailoutImage, y, 2);

	QVERIFY(primaryPixels > 100);
	QVERIFY(bailoutPixels > 20);
	QVERIFY2(primaryMaxGap <= 2,
		 "Primary profile line is expected to remain visually solid.");
	QVERIFY2(bailoutMaxGap >= primaryMaxGap + 3,
		 "Bailout profile line is expected to contain visible dash gaps on flat segments.");
}

void TestProfile::testOcBailoutGasChangeEventsAreDrawn()
{
	prefs = default_prefs;
	prefs.show_bailout_in_planner = true;

	auto primaryDive = createOcDiveWithSamples(10, 0);
	auto bailoutDive = createOcDiveWithSamples(5, 35);

	DivePlannerPointsModel *plannerModel = DivePlannerPointsModel::instance();
	plannerModel->resetPlanState();
	diveplan &dp = plannerModel->getDiveplan();
	dp.show_bailout = true;
	dp.bailout_scenario = SCENARIO_OC_BAILOUT;
	dp.bailout_gflow = 85;
	dp.bailout_gfhigh = 95;
	dp.bailout_dive = std::move(bailoutDive);

	ProfileScene scene(1.0, false, false);
	scene.resize(QSizeF(900.0, 500.0));
	scene.plotDive(primaryDive.get(), 0, plannerModel, true, true, false, true);

	int eventCount = 0;
	for (QGraphicsItem *item: scene.items()) {
		if (dynamic_cast<DiveEventItem *>(item))
			++eventCount;
	}

	QVERIFY2(eventCount > 0,
		 "Expected gas-change event markers in OC bailout overlay; this guards against using the wrong plot-info source.");
}

// Stubs for command/map symbols referenced through qt-models in this test target.
namespace Command {
void addDive(std::unique_ptr<dive>, bool, bool) {}
void importDives(struct divelog *, int, const QString &) {}
void replanDive(dive *) {}
int editCylinder(int, cylinder_t, EditCylinderType, bool) { return 0; }
void editSensors(int, int, int) {}
void editDiveSiteName(dive_site *, const QString &) {}
void editDiveSiteDescription(dive_site *, const QString &) {}
void removePictures(const std::vector<PictureListForDeletion> &) {}
int editWeight(int, weightsystem_t, bool) { return 0; }
}

#ifdef MAP_SUPPORT
MapWidget *MapWidget::m_instance = nullptr;
MapWidget *MapWidget::instance()
{
	return m_instance;
}

bool MapWidget::editMode() const
{
	return false;
}

void MapWidget::reload()
{
}

void MapWidget::setSelected(std::vector<dive_site *>)
{
}

MainWindow *MainWindow::instance()
{
	return nullptr;
}
#endif

QTEST_MAIN(TestProfile)
