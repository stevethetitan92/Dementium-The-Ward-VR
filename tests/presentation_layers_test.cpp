#include "../src/xr_session.hpp"
#include <cassert>
#include <cstdio>
static unsigned releases,expectedLayers;static bool expectProjection,expectFade;
static XrResult XRAPI_PTR releaseImage(XrSwapchain,const XrSwapchainImageReleaseInfo*){++releases;return XR_SUCCESS;}
static XrResult XRAPI_PTR endFrame(XrSession,const XrFrameEndInfo* e){
 assert(releases==4);assert(e->displayTime==123456);assert(e->layerCount==expectedLayers);
 if(expectProjection){assert(e->layers[0]->type==XR_TYPE_COMPOSITION_LAYER_PROJECTION);auto p=reinterpret_cast<const XrCompositionLayerProjection*>(e->layers[0]);assert(p->viewCount==2);assert(p->views[0].pose.position.x==-.032f);assert(p->views[1].pose.position.x==.032f);}
 if(expectFade){auto q=reinterpret_cast<const XrCompositionLayerQuad*>(e->layers[e->layerCount-1]);assert(q->type==XR_TYPE_COMPOSITION_LAYER_QUAD);assert(q->eyeVisibility==XR_EYE_VISIBILITY_BOTH);assert(q->layerFlags&XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT);assert(q->subImage.imageRect.extent.width==1);}
 return XR_SUCCESS;
}
int main(){for(bool rendering:{false,true})for(bool world:{false,true})for(bool hud:{false,true})for(float alpha:{0.f,.5f,1.f}){
 XrSessionBridge xr;xr.frameBegun=true;xr.render=rendering;xr.predictedTime=123456;
 xr.xrReleaseSwapchainImage=releaseImage;xr.xrEndFrame=endFrame;
 for(unsigned i=0;i<2;++i){xr.eyes[i].acquired=true;xr.eyes[i].view.pose.orientation.w=1;xr.eyes[i].view.pose.position.x=i?.032f:-.032f;}
 xr.overlay.acquired=xr.fade.acquired=true;releases=0;
 expectProjection=rendering&&world;expectFade=rendering&&(world||hud)&&alpha>0;
 expectedLayers=unsigned(expectProjection)+unsigned(rendering&&hud)+unsigned(expectFade);
 xr.endFrame(world,hud,true,alpha);assert(!xr.frameBegun&&!xr.render);assert(!xr.fade.acquired&&!xr.overlay.acquired);
 }puts("PASS: 24 actual endFrame layer combinations; stereo poses, flat-screen fade, release order, no runtime");}
