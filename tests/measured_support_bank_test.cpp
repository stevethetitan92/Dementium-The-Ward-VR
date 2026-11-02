#include <windows.h>
#include <cstring>
#include <algorithm>
#include "../src/tracking_math.hpp"
#include "../src/replacement_hands.hpp"
#include "../src/hand_fitting.hpp"
#include <stdexcept>
static void require(bool v){if(!v)throw std::runtime_error("measured support hand bank failed");}
int main(int argc,char** argv){try{
    require(argc==3);require(replacementHands::load(argv[1]));require(replacementHands::sections==64);
    FILE* file=nullptr;fopen_s(&file,argv[2],"rb");require(file!=nullptr);unsigned cases{};
    require(fread(&cases,4,1,file)==1&&cases==3);float worst=0;unsigned vertices=0;
    for(unsigned c=0;c<cases;c++){
        unsigned weapon{},count{};require(fread(&weapon,4,1,file)==1&&fread(&count,4,1,file)==1&&count==504);
        require(replacementHands::measuredSupport(static_cast<int>(weapon)));auto section=replacementHands::supportSection(static_cast<int>(weapon));
        require(replacementHands::count[section]==534);require(!replacementHands::canMorph(section,2));require(!replacementHands::canMorph(2,section));
        require(replacementHands::canMorph(section,section+2));
        XrPosef pose{},identity{{0,0,0,1},{0,0,0}};require(handFitting::measuredSupportPose(identity,static_cast<int>(weapon),pose));
        auto m=motionTracking::rotation(pose.orientation)*DirectX::XMMatrixTranslation(pose.position.x,pose.position.y,pose.position.z);
        for(unsigned i=0;i<count;i++){
            float expected[3];require(fread(expected,sizeof expected,1,file)==1);
            const auto& v=replacementHands::geometry[replacementHands::first[section]+i];DirectX::XMFLOAT3 actual;
            DirectX::XMStoreFloat3(&actual,DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(v.x,v.y,v.z,1),m));
            const float values[]={actual.x,actual.y,actual.z};for(unsigned k=0;k<3;k++){const float error=std::fabs(values[k]-expected[k]);worst=(std::max)(worst,error);require(error<.0002f);}
            ++vertices;
        }
    }require(fgetc(file)==EOF);fclose(file);
    require(!replacementHands::measuredSupport(1));require(!replacementHands::canMorph(64,0));
    std::printf("PASS: 64-section bank loaded; %u actual native support vertices match runtime fit (max error %.9g); incompatible topology never morphs.\n",vertices,worst);return 0;
}catch(const std::exception& e){std::puts(e.what());return 1;}}
