#pragma once
#include "wristband_model.hpp"
#include "wristband_number_glyphs.hpp"
#include <vector>
#include <string>
#include <cstdio>
namespace wristband {
struct Vertex{float x,y,z,nx,ny,nz,r,g,b,u,v,print;};
static_assert(sizeof(Vertex)==48,"Wristband vertex/export layout must match the preview and input layout");
struct Color{float r,g,b;};
inline constexpr Color ivory{.843f,.827f,.769f},ink{.204f,.224f,.196f},olive{.439f,.514f,.416f},red{.32f,.13f,.10f};
inline constexpr float pi=3.14159265359f;
inline Vertex surface(float theta,float z,float radial,Color c,float printed=0){
    // Measured enclosing wrist section; translated in the actual hand-bone frame.
    float rx=settings.radiusX,ry=settings.radiusY;
    float x=std::sin(theta),y=std::cos(theta);float nx=x/rx,ny=y/ry,n=std::sqrt(nx*nx+ny*ny);
    return {x*(rx+radial)+fittedCenter.x,y*(ry+radial)+fittedCenter.y,z+fittedCenter.z,nx/n,ny/n,0,c.r,c.g,c.b,theta/(2*pi)+.25f,z/settings.width+.5f,printed};
}
inline void quad(std::vector<Vertex>& out,Vertex a,Vertex b,Vertex c,Vertex d){out.insert(out.end(),{a,b,c,a,c,d});}
inline std::vector<Vertex> bandMesh(){
    std::vector<Vertex> out;out.reserve(96*8*6);
    // Closed rounded rectangular cross section: outer/inner walls and both
    // lips are one indexed-equivalent continuous loop, with no open ends.
    float w=settings.width*.5f,t=.00075f,b=.00025f;
    const float section[8][2]={{t-b,-w},{t,-w+b},{t,w-b},{t-b,w},{-t+b,w},{-t,w-b},{-t,-w+b},{-t+b,-w}};
    for(unsigned i=0;i<96;i++)for(unsigned j=0;j<8;j++){
        float a=pi*.5f+2*pi*i/96,c=pi*.5f+2*pi*(i+1)/96;unsigned k=(j+1)%8;
        auto v0=surface(a,section[j][1],section[j][0],ivory),v1=surface(c,section[j][1],section[j][0],ivory),v2=surface(c,section[k][1],section[k][0],ivory),v3=surface(a,section[k][1],section[k][0],ivory);
        if(j>=3){for(auto v:{&v0,&v1,&v2,&v3})v->print=-1;}
        if(j>=4){for(auto v:{&v0,&v1,&v2,&v3}){v->nx=-v->nx;v->ny=-v->ny;}}
        if(j==3||j==7){for(auto v:{&v0,&v1,&v2,&v3}){v->nx=v->ny=0;v->nz=j==3?1.f:-1.f;}}
        quad(out,v0,v1,v2,v3);
    }return out;
}
inline void patch(std::vector<Vertex>& out,float face,float x,float y,float width,float height,Color color){
    y*=settings.width/.038f;height*=settings.width/.038f;
    unsigned count=(std::max)(1u,static_cast<unsigned>(std::ceil(width/.0015f)));
    for(unsigned i=0;i<count;i++){float a=x+width*i/count,b=x+width*(i+1)/count;quad(out,surface(face+a/.033f,y,.00086f,color,1),surface(face+b/.033f,y,.00086f,color,1),surface(face+b/.033f,y+height,.00086f,color,1),surface(face+a/.033f,y+height,.00086f,color,1));}
}
inline void cross(std::vector<Vertex>& out,float face,float x,float y,float size,Color c){patch(out,face,x+size*.33f,y,size*.34f,size,c);patch(out,face,x,y+size*.33f,size,size*.34f,c);}

// Raised overlap strap with genuine open perforations and a snap stud.
// Each solid cell has both skins; only boundary cells receive side walls.
inline std::vector<Vertex> printedMesh(){
    std::vector<Vertex> out;constexpr int cols=12,rows=28;const float step=.001f;
    auto solid=[=](int x,int y){if(x<0||x>=cols||y<0||y>=rows)return false;
        float u=(x+.5f-cols*.5f)*.001f,z=(y+.5f-rows*.5f)*.001f;
        if(std::fabs(u)>.0045f&&std::fabs(z)>.0125f)return false;
        for(float center:{-.008f,0.f,.008f})if(u*u+(z-center)*(z-center)<.00165f*.00165f)return false;
        return true;};
    auto v=[](float x,float z,float radial){auto p=surface(-pi*.5f+x/settings.radiusX,z,radial,ivory,-1);return p;};
    for(int x=0;x<cols;x++)for(int y=0;y<rows;y++)if(solid(x,y)){
        float a=(x-cols*.5f)*step,b=a+step,c=(y-rows*.5f)*step,d=c+step;
        float lo=.001f,hi=.0017f;
        quad(out,v(a,c,hi),v(b,c,hi),v(b,d,hi),v(a,d,hi));
        quad(out,v(a,d,lo),v(b,d,lo),v(b,c,lo),v(a,c,lo));
        if(!solid(x-1,y))quad(out,v(a,c,lo),v(a,c,hi),v(a,d,hi),v(a,d,lo));
        if(!solid(x+1,y))quad(out,v(b,d,lo),v(b,d,hi),v(b,c,hi),v(b,c,lo));
        if(!solid(x,y-1))quad(out,v(b,c,lo),v(b,c,hi),v(a,c,hi),v(a,c,lo));
        if(!solid(x,y+1))quad(out,v(a,d,lo),v(a,d,hi),v(b,d,hi),v(b,d,lo));
    }
    Color metal{.23f,.22f,.18f};
    for(unsigned j=0;j<32;j++){
        float a=2*pi*j/32,b=2*pi*(j+1)/32;
        auto center=surface(-pi*.5f,-.008f,.0022f,metal,2);
        auto A=surface(-pi*.5f+std::cos(a)*.0012f/settings.radiusX,-.008f+std::sin(a)*.0012f,.0022f,metal,2);
        auto B=surface(-pi*.5f+std::cos(b)*.0012f/settings.radiusX,-.008f+std::sin(b)*.0012f,.0022f,metal,2);
        out.insert(out.end(),{center,A,B});
    }return out;
}
inline void numberText(std::vector<Vertex>& out,float face,float x,float z,const char* value,float height,Color color=ink){
    for(const char* c=value;*c;c++){
        if(*c==' '){x+=height*.20f;continue;}
        for(auto& g:numberGlyphs)if(g.code==*c){
            for(unsigned i=0;i<g.count;i++){const auto& r=numberRects[g.start+i];patch(out,face,x+r.x*height,z+r.y*height,r.w*height,r.h*height,color);}
            x+=height*g.advance;break;
        }
    }
}
inline Color healthColor(const Snapshot& s){
    // User video "Video Project 47.mp4": 80/90/100 green,
    // 50/60/70 yellow, 0/10/20/30/40 red. Native health uses ten steps.
    // RGB approximates the bright ink sampled from the compressed video;
    // these are not claimed to be original uncompressed texture values.
    if(!s.healthValid||s.maximum<=0)return ivory;
    const float fill=healthFill(s);
    if(fill>=.8f)return {2/255.f,189/255.f,8/255.f};
    if(fill>=.5f)return {228/255.f,215/255.f,15/255.f};
    return {224/255.f,26/255.f,5/255.f};
}
inline void ekgMesh(std::vector<Vertex>& out,Color color){
    // Cover the old baked waveform; new waveform has equal top/bottom margins.
    patch(out,pi,-.034f,-.014f,.009f,.008f,{95/255.f,30/255.f,24/255.f});
    constexpr float points[][2]={{-.033f,-.010f},{-.031f,-.010f},{-.030f,-.012f},
        {-.029f,-.007f},{-.028f,-.013f},{-.027f,-.010f},{-.026f,-.010f}};
    for(unsigned i=1;i<7;++i){
        const float ax=points[i-1][0],ay=points[i-1][1],bx=points[i][0],by=points[i][1];
        const float dx=bx-ax,dy=by-ay,length=std::sqrt(dx*dx+dy*dy);
        const float ox=-dy/length*.00018f,oy=dx/length*.00018f;
        auto point=[&](float x,float y){return surface(pi+x/.033f,y*settings.width/.038f,.00096f,color,1);};
        // A long straight triangle cuts through the curved band even when its
        // endpoints are above it. Follow the surface so the pulse stays solid.
        const unsigned steps=(std::max)(1u,static_cast<unsigned>(std::ceil(length/.00025f)));
        for(unsigned j=0;j<steps;++j){
            const float t0=float(j)/steps,t1=float(j+1)/steps;
            const float x0=ax+dx*t0,y0=ay+dy*t0,x1=ax+dx*t1,y1=ay+dy*t1;
            quad(out,point(x0+ox,y0+oy),point(x1+ox,y1+oy),point(x1-ox,y1-oy),point(x0-ox,y0-oy));
        }
    }
}
inline std::vector<Vertex> statusMesh(const Snapshot& s){
    std::vector<Vertex> out;out.reserve(18000);char number[32];
    ekgMesh(out,healthColor(s));
    if(s.healthValid){float fill=healthFill(s);if(fill>0)patch(out,pi,-.0105f,-.0125f,.025f*fill,.003f,healthColor(s));
        std::snprintf(number,sizeof number,"%d%%",static_cast<int>(std::lround(healthFill(s)*100.f)));
    }else std::snprintf(number,sizeof number,"-");
    numberText(out,pi,.018f,-.013f,number,.0035f);
    if(s.ammoValid)std::snprintf(number,sizeof number,"%d - %d",s.loaded,s.reserve);else std::snprintf(number,sizeof number,"-");
    numberText(out,pi,.002f,-.0035f,number,.0042f);
    return out;
}
}
