#include "../src/shotgun_pump_cursor.hpp"
#include <cstdio>
#include <stdexcept>
#define check(b) do{if(!(b)){printf("line %d\n",__LINE__);throw std::runtime_error("pump cursor failed");}}while(0)
int main(){try{
 using namespace selectedShotgun;Motion m;m.ready=true;
 for(auto pair:{std::pair<float,float>{0,0},{.2f,0},{.5f,.5f},{.7f,.5f},{1,1},{1.2f,1},{1.7f,0}}){Key k;k.seconds=pair.first;k.pump=pair.second;m.keys.push_back(k);}
 check(PumpCursor::validCycle(m));PumpCursor c;
 check(std::fabs(c.update(m,1,.1f,true)-.1f)<1e-6f);check(!c.complete());
 check(std::fabs(c.update(m,1,.1f,true)-.5f)<1e-6f); // initial dwell done, midpoint dwell begins
 check(std::fabs(c.update(m,1,.1f,true)-.6f)<1e-6f);
 c.update(m,1,.1f,true);check(c.time()>=1&&c.time()<1.01f);
 c.update(m,1,.1f,true);check(c.time()<1.2f);
 c.update(m,1,.1f,true);check(std::fabs(c.time()-1.2f)<1e-5f&&!c.complete());
 c.update(m,.5f,.016f,true);check(std::fabs(c.time()-1.45f)<1e-5f);
 c.update(m,.8f,.016f,true);check(std::fabs(c.time()-1.3f)<1e-5f&&!c.complete());
 c.update(m,0,.016f,true);check(c.complete()&&std::fabs(c.time()-1.7f)<1e-5f);
 c.update(m,0,.016f,false);check(c.time()==0&&!c.complete());
 c.update(m,.25f,.2f,true);check(c.time()<.5f); // cannot jump past unpulled segment
 for(unsigned i=0;i<30;i++)c.update(m,.75f,.016f,true);
 check(c.time()>.7f&&c.time()<1);
 c.update(m,.25f,.016f,true);check(std::fabs(c.time()-.35f)<1e-5f&&!c.complete());
 c.update(m,0,.016f,true);check(std::fabs(c.time()-.2f)<1e-5f&&!c.complete());
 c.update(m,.5f,1,true);check(c.time()==0); // stale frame does not complete motion
 m.keys[4].pump=.4f;check(!PumpCursor::validCycle(m));
 puts("PASS: pull/return branch, authored dwell, held position and invalid/reset behavior");return 0;
 }catch(const std::exception&e){puts(e.what());return 1;}}

