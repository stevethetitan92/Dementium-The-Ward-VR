#define DEMENTIUM_PROFILE_DRAWS
#include "../src/draw_cost_profile.hpp"
#include <stdexcept>
#include <cstdio>
int main(){try{
 const double budget=1000./90.;
 for(double ms:{10.,budget,11.2,16.7,125.})drawCost::recordInterval(ms,budget);
 auto w=drawCost::takeFrames();
 if(w.count!=5||w.over60HzBudget!=2||w.overRuntimeBudget!=3||w.maximum!=125.||w.p95Upper!=125.||w.p99Upper!=125.)throw std::runtime_error("runtime/60Hz classification or overflow failed");
 auto empty=drawCost::takeFrames();if(empty.count||empty.p95Upper||empty.p99Upper||empty.maximum||empty.overRuntimeBudget)throw std::runtime_error("empty/reset failed");
 drawCost::recordInterval(12.,0);w=drawCost::takeFrames();if(w.overRuntimeBudget||w.p99Upper!=12.5)throw std::runtime_error("unavailable runtime or bound failed");
 puts("PASS: 90Hz versus 60Hz budgets, exact boundary, overflow, empty/reset, unknown runtime. Interval overruns are not compositor dropped-frame counts.");return 0;
 }catch(const std::exception& e){puts(e.what());return 1;}}
