#include "../src/wristband_native.hpp"
#include "../src/wristband_renderer.hpp"
#include <d3d12shader.h>
#include <fstream>
#include <map>
#include <stdexcept>
#include <thread>
void check(bool b){if(!b)throw std::runtime_error("wristband validation failed");}
int main(){try{
 using namespace wristband;
 {Microsoft::WRL::ComPtr<ID3DBlob> code,error;check(SUCCEEDED(D3DCompile(nativeHandClip::shader,sizeof(nativeHandClip::shader)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&code,&error)));
 Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;check(SUCCEEDED(D3DReflect(code->GetBufferPointer(),code->GetBufferSize(),IID_PPV_ARGS(&reflection))));
 D3D12_SHADER_BUFFER_DESC desc{};auto buffer=reflection->GetConstantBufferByName("Globals");check(SUCCEEDED(buffer->GetDesc(&desc))&&desc.Size==1200);
 D3D12_SHADER_VARIABLE_DESC variable{};check(SUCCEEDED(buffer->GetVariableByName("hideBones")->GetDesc(&variable))&&variable.StartOffset==1192);
 nativeHandClip::drawn[0]=nativeHandClip::drawn[1]=1000;check(nativeHandClip::mask(1,1000)==(0x4000001eu|3072));check(nativeHandClip::mask(1,1100)==0);check(nativeHandClip::mask(7,1000)==0);
 nativeHandClip::diagnosticUntil=2000;check(nativeHandClip::mask(1,1500)==(0x4000001eu|3072));check(nativeHandClip::mask(1,2000)==0);nativeHandClip::diagnosticUntil=0;
 nativeHandClip::vrGameplay=true;check(nativeHandClip::mask(1,5000)==(0x4000001eu|3072));check(nativeHandClip::mask(7,5000)==0);nativeHandClip::vrGameplay=false;check(nativeHandClip::mask(1,5000)==0);
 nativeHandClip::originalHands=true;check(nativeHandClip::mask(1,1000)==(2|1024));check(nativeHandClip::mask(1,1100)==0);check(nativeHandClip::mask(2,1000)==2);nativeHandClip::originalHands=false;
 {nativeHandClip::Scope scoped(28);check(nativeHandClip::currentMask==28);}check(nativeHandClip::currentMask==0);}
 check(replacementHands::load("assets/hands/FloatingHands.bin"));
 check(replacementHands::geometry.size()>200000&&replacementHands::count[0]==replacementHands::count[1]);
 check(replacementHands::section(handGrip::Pose::Bottle,0)==6&&replacementHands::section(handGrip::Pose::Mechanism,1)==9);
 check(!replacementHands::load("assets/hands/missing.bin")&&!replacementHands::ready);
 check(replacementHands::load("assets/hands/FloatingHands.bin"));
 check(huntingRifle::load("assets/hunting-rifle/HuntingRifle.bin"));
 check(huntingRifle::geometry.size()==3957&&huntingRifle::count[2]>0);
 manualReload::Hands rifleHands;rifleHands.session=rifleHands.gameplay=rifleHands.gunTracked=true;rifleHands.weapon=4;rifleHands.time=1000;
 check(huntingRifle::eligible(rifleHands,1000));check(!huntingRifle::eligible(rifleHands,1100));
 rifleHands.gameplay=false;check(!huntingRifle::eligible(rifleHands,1000));rifleHands.gameplay=true;
 huntingRifle::prepare(rifleHands,1000);check(huntingRifle::visible&&huntingRifle::partVisible[0]&&huntingRifle::partVisible[2]&&!huntingRifle::partVisible[3]);
 check(!huntingRifle::replaceNative(4));
 
 for(unsigned d=0;d<=10;d++){auto s=decode(d,1,6,24,1000);check(s.healthValid&&s.health==10-int(d)&&s.maximum==10&&std::fabs(healthFill(s)-(10-d)/10.f)<.00001f&&s.ammoValid&&!s.medsValid);}
 check(!decode(255,1,6,24,1000).healthValid);check(!decode(0,6,0,0,1000).ammoValid);check(!decode(0,0,7,24,1000).ammoValid);
 auto s=decode(5,1,0,0,1000);Model m;m.publish(s);check(m.read(1000).revision==1);s.time=1050;m.publish(s);check(m.read(1050).revision==1);
 check(!m.read(1300).world&&!m.read(999).healthValid);s.meds=3;s.medsValid=true;m.publish(s);check(m.read(1050).medsValid);s.meds=4;check(!sanitize(s).medsValid);
 s.maximum=0;check(!sanitize(s).healthValid&&healthFill(sanitize(s))==0);
 // Typed snapshots remain internally consistent under concurrent publication.
 Model shared;std::thread writer([&]{for(unsigned i=0;i<20000;i++){Snapshot v;v.time=1;v.loaded=i%7;v.reserve=24-v.loaded;v.weapon=0;v.ammoValid=true;shared.publish(v);}});
 for(unsigned i=0;i<20000;i++){auto v=shared.read(1);if(v.ammoValid)check(v.loaded+v.reserve==24);}writer.join();
 auto band=bandMesh();check(band.size()==96*8*6);auto print=printedMesh();check(print.size()>1000&&print.size()<100000);
 using Point=std::array<long,3>;using Edge=std::pair<Point,Point>;std::map<Edge,unsigned> edges;
 auto key=[](const Vertex& v){return Point{std::lround(v.x*1000000),std::lround(v.y*1000000),std::lround(v.z*1000000)};};
 for(size_t i=0;i<band.size();i+=3)for(unsigned j=0;j<3;j++){auto a=key(band[i+j]),b=key(band[i+(j+1)%3]);if(b<a)std::swap(a,b);++edges[{a,b}];}
 for(auto& e:edges)check(e.second==2);
 check(weaponGripHands::load(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\rom.zar"));
 // Both original forearm extensions are closed, short, and share their material.
 for(unsigned hand=0;hand<2;hand++){
  auto arm=forearmMesh(hand);check(arm.size()==64*6*6+64*2*3);edges.clear();
  for(size_t i=0;i<arm.size();i+=3)for(unsigned j=0;j<3;j++){auto a=key(arm[i+j]),b=key(arm[i+(j+1)%3]);if(b<a)std::swap(a,b);++edges[{a,b}];}
  for(auto& e:edges)check(e.second==2);
  for(auto& v:arm){check(v.z>=forearmStart-.00001f&&v.z<=forearmEnd+.00001f&&v.print==5&&v.r==1.f&&std::isfinite(v.nx));
   if(hand==0&&v.z>=fittedCenter.z-settings.width*.5f&&v.z<=fittedCenter.z+settings.width*.5f){float x=(v.x-fittedCenter.x)/(settings.radiusX-.00075f),y=(v.y-fittedCenter.y)/(settings.radiusY-.00075f);check(x*x+y*y<1);}}
  std::string stem=hand?"assets/wristband/ForearmRight":"assets/wristband/ForearmLeft";
  std::ofstream binary(stem+".mesh",std::ios::binary);binary.write(reinterpret_cast<const char*>(arm.data()),arm.size()*sizeof(Vertex));check(binary.good());
  std::ofstream model(stem+".obj");model<<"# Original short forearm extension; metres.\nmtllib RedmoorWristband.mtl\nusemtl NeutralHandMaterial\n";
  for(auto& v:arm)model<<"v "<<v.x<<' '<<v.y<<' '<<v.z<<'\n';for(auto& v:arm)model<<"vn "<<v.nx<<' '<<v.ny<<' '<<v.nz<<'\n';
  for(size_t i=0;i<arm.size();i+=3)model<<"f "<<i+1<<"//"<<i+1<<' '<<i+2<<"//"<<i+2<<' '<<i+3<<"//"<<i+3<<'\n';check(model.good());
 }
 for(auto& v:band)check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::fabs(v.z-fittedCenter.z)<=settings.width*.5f+.00001f);
 // Reuse the locally owned pistol hand; no proprietary mesh is embedded in the mod.
 check(weaponGripHands::source.size()==200*3);
 for(unsigned variant=0;variant<3;variant++){
  auto& geometry=weaponGripHands::meshes[variant];check(geometry.size()>3000&&geometry.size()<10000);
  for(auto& v:geometry)check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::fabs(v.x)<2&&std::fabs(v.y)<2&&std::fabs(v.z)<2&&v.print==5);
  manualReload::Hands hands;hands.weapon=int(variant+2);hands.session=hands.gameplay=hands.gunTracked=true;hands.time=1000;hands.scale=1;hands.gun={{0,0,0,1},{0,0,-.4f}};
  auto& capture=weaponGripHands::captures[variant];capture.body={{.4958496f,-.4958496f,.5041504f,.5041504f},arsenal::profiles[variant+1].anchor};capture.time=1000;capture.valid=true;
  DirectX::XMMATRIX matrix;check(weaponGripHands::frame(variant,hands,1000,matrix));check(!weaponGripHands::frame(variant,hands,1100,matrix));hands.gameplay=false;check(!weaponGripHands::frame(variant,hands,1000,matrix));hands.gameplay=true;hands.gunTracked=false;check(!weaponGripHands::frame(variant,hands,1000,matrix));
  const char* names[]={"rifle","shotgun","sniper"};std::ofstream file(std::string("../../work/grip-fit-")+names[variant]+".mesh",std::ios::binary);file.write(reinterpret_cast<const char*>(geometry.data()),geometry.size()*sizeof(Vertex));check(file.good());
 }
 check(!weaponGripHands::decode({},{}));
 auto top=surface(0,0,.0013f,ink),bottom=surface(pi,0,.0013f,ink);check(top.y>0&&bottom.y<0&&top.ny>0&&bottom.ny<0);
 XrPosef grip{{0,0,0,1},{0,0,-.4f}},head{{0,0,0,1},{}};auto left=bind(grip,0),right=bind(grip,1);check(motionTracking::validPose(left)&&motionTracking::validPose(right));
 check(std::fabs(left.position.z+.4f)<.00001f&&inspect(left,head,false));grip.orientation={0,0,1,0};auto rolled=bind(grip,0);check(std::fabs(rolled.orientation.x-left.orientation.x)>.1f); // no billboarding
 check(!inspect({{0,0,0,1},{0,0,.4f}},head,false));
 Microsoft::WRL::ComPtr<ID3DBlob> vs,ps,error;
 check(SUCCEEDED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,&error)));
 check(SUCCEEDED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&ps,&error)));
 // Embedded original albedo decodes without the game or a GPU session.
 HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);check(SUCCEEDED(com));
 Microsoft::WRL::ComPtr<IWICImagingFactory> factory;Microsoft::WRL::ComPtr<IWICStream> stream;Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> image;
 check(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))));
 check(SUCCEEDED(factory->CreateStream(&stream)));check(SUCCEEDED(stream->InitializeFromMemory(const_cast<BYTE*>(albedoPNG),sizeof(albedoPNG))));
 check(SUCCEEDED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder)));check(SUCCEEDED(decoder->GetFrame(0,&image)));
 UINT imageWidth{},imageHeight{};check(SUCCEEDED(image->GetSize(&imageWidth,&imageHeight))&&imageWidth==1536&&imageHeight==384);
 image.Reset();decoder.Reset();stream.Reset();factory.Reset();CoUninitialize();
 Snapshot largest;largest.healthValid=largest.ammoValid=largest.medsValid=true;largest.health=10;largest.maximum=10;largest.loaded=30;largest.reserve=255;largest.meds=3;
 check(statusMesh(largest).size()<60000);largest.health=0;check(healthFill(largest)==0);
 // Relocate executable bytes as inert data and check the adapter's signatures.
 std::ifstream f("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\Dementium.exe",std::ios::binary);
 std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});check(bytes.size()>4096);
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data()+dos->e_lfanew);
 std::vector<unsigned char> mapped(nt->OptionalHeader.SizeOfImage);auto sections=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++){auto& sec=sections[i];std::memcpy(mapped.data()+sec.VirtualAddress,bytes.data()+sec.PointerToRawData,sec.SizeOfRawData);}
 auto base=reinterpret_cast<std::uintptr_t>(mapped.data());*reinterpret_cast<std::uintptr_t*>(base+0x30c0f8+0x20)=base+0x867e0;
 check(verify(base));for(const auto& g:guards){mapped[g.rva]^=1;check(!verify(base));mapped[g.rva]^=1;}
 // Export original mod geometry, including fixed print, without sample counters.
 band.insert(band.end(),print.begin(),print.end());Snapshot unknown;auto counters=statusMesh(unknown);band.insert(band.end(),counters.begin(),counters.end());
 std::ofstream mesh("assets/wristband/RedmoorWristband.mesh",std::ios::binary);mesh.write(reinterpret_cast<const char*>(band.data()),band.size()*sizeof(Vertex));check(mesh.good());
 std::ofstream obj("assets/wristband/RedmoorWristband.obj");obj<<"# Original procedural wristband; metres; UVs; vertex colours.\nmtllib RedmoorWristband.mtl\nusemtl AgedIvory\n";
 for(auto& v:band)obj<<"v "<<v.x<<' '<<v.y<<' '<<v.z<<' '<<v.r<<' '<<v.g<<' '<<v.b<<'\n';
 for(auto& v:band)obj<<"vt "<<v.u<<' '<<1-v.v<<'\n';for(auto& v:band)obj<<"vn "<<v.nx<<' '<<v.ny<<' '<<v.nz<<'\n';
 int material=-1;for(size_t i=0;i<band.size();i+=3){const auto& v=band[i];int next=v.print<0?1:(v.print<.5f?0:(v.print>1.5f?3:2));
 if(next!=material){material=next;obj<<"usemtl "<<(next==0?"PrintedPlastic":next==1?"AgedIvory":next==2?"StatusInk":"SnapStud")<<'\n';}
 obj<<"f";for(unsigned j=1;j<=3;j++)obj<<' '<<i+j<<'/'<<i+j<<'/'<<i+j;obj<<'\n';}
 check(obj.good());
 puts("PASS: wristband closed manifold, opposite faces, printed geometry, read-only status validity/staleness, threaded snapshots, pose binding and shaders; game not launched.");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
