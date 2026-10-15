// SPDX-License-Identifier: GPL-2.0
#ifndef PLANNER_H
#define PLANNER_H

#include "units.h"
#include "divemode.h"
#include <memory>
#include <string>
#include <vector>

struct dive;

/* this should be converted to use our types */
struct divedatapoint {
	int time = 0;
	depth_t depth;
	int cylinderid = 0;
	pressure_t minimum_gas;
	int setpoint = 0;
	bool entered = false;
	enum divemode_t divemode = OC;

	divedatapoint() = default;
	divedatapoint(const divedatapoint &) = default;
	divedatapoint(divedatapoint &&) = default;
	divedatapoint &operator=(const divedatapoint &) = default;
	divedatapoint(int time_incr, depth_t depth, int cylinderid, int po2, bool entered);
};

typedef enum {
	PLAN_OK,
	PLAN_ERROR_TIMEOUT,
	PLAN_ERROR_INAPPROPRIATE_GAS,
	PLAN_ERROR_NO_SUITABLE_BAILOUT_GAS,
	PLAN_ERROR_RECREATIONAL_EXCEEDS_NDL,
} planner_error_t;

/*
 * The deco "views" the planner can compute and the profile can display:
 *  - PRIMARY:     the dive as entered (closed circuit on the loop for a CCR dive, open circuit otherwise)
 *  - CCR_BAILOUT: a CCR dive whose ascent is done on open circuit bailout gas, using the bailout gradient factors
 *  - OC_BAILOUT:  an open circuit dive whose ascent is recomputed with the bailout gradient factors
 */
enum plan_scenario {
	SCENARIO_PRIMARY,
	SCENARIO_CCR_BAILOUT,
	SCENARIO_OC_BAILOUT,
};

/* Which alternative (bailout) scenario applies to a dive that is done in the given dive mode. */
extern plan_scenario bailout_scenario_for_mode(enum divemode_t divemode);

struct diveplan {
	diveplan();
	~diveplan();
	diveplan(const diveplan &) = default;
	diveplan(diveplan &&) = default;
	diveplan &operator=(const diveplan &) = default;
	diveplan &operator=(diveplan &&) = default;

	timestamp_t when = 0;
	pressure_t surface_pressure;
	int bottomsac = 0;	/* ml/min */
	int decosac = 0;	  /* ml/min */
	int salinity = 0;
	short gflow = 0;
	short gfhigh = 0;
	short vpmb_conservatism = 0;
	std::vector<divedatapoint> dp;
	std::vector<divedatapoint> backup_dp;	/* the calculated (non-entered) segments of the bailout ascent */
	int bailout_gflow = 0, bailout_gfhigh = 0;
	bool show_bailout = false;
	plan_scenario bailout_scenario = SCENARIO_PRIMARY;	/* scenario of bailout_dive, SCENARIO_PRIMARY if there is none */
	std::shared_ptr<struct dive> bailout_dive;	/* full dive (entered part + bailout ascent), if show_bailout is set */
	int eff_gflow = 0, eff_gfhigh = 0;
	int surface_interval = 0;

	bool is_empty() const;
	void add_plan_to_notes(struct dive &dive, bool show_disclaimer, planner_error_t error);
	int duration() const;
};

struct deco_state_cache;

extern int get_cylinderid_at_time(struct dive *dive, struct divecomputer *dc, duration_t time);
extern int ascent_velocity(depth_t depth, depth_t avg_depth, int);
extern const char *get_planner_disclaimer();

void plan_add_segment(struct diveplan &diveplan, int duration, depth_t depth, int cylinderid, int po2, bool entered, enum divemode_t divemode);
#if DEBUG_PLAN
void dump_plan(struct diveplan *diveplan);
#endif
struct decostop {
	int depth;
	int time;
};

extern std::string get_planner_disclaimer_formatted();
extern planner_error_t plan(struct deco_state *ds, struct diveplan &diveplan, struct dive *dive, int dcNr, int timestep, deco_state_cache &cache, bool is_planner, bool show_disclaimer, std::vector<decostop> *decostoptable, plan_scenario scenario = SCENARIO_PRIMARY);
#endif // PLANNER_H
