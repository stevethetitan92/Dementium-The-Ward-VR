#pragma once
#include <array>
namespace saveSlots {
enum class MenuPage { closed, slots, overwrite, error };
struct MenuAction {bool selected{},fresh{},overwrite{};unsigned slot{};};
class Menu {
 MenuPage page_=MenuPage::closed;unsigned selected_=0;bool fresh_=false,confirmOverwrite_=false;
public:
 MenuPage page()const{return page_;}unsigned selection()const{return selected_;}bool newGame()const{return fresh_;}bool confirmOverwrite()const{return confirmOverwrite_;}
 void open(bool fresh,unsigned active){fresh_=fresh;selected_=active<3?active:0;confirmOverwrite_=false;page_=MenuPage::slots;}
 void move(int delta){if(page_==MenuPage::slots)selected_=unsigned((int(selected_)+(delta>0?1:2))%3);else if(page_==MenuPage::overwrite)confirmOverwrite_=!confirmOverwrite_;}
 void back(){if(page_==MenuPage::overwrite||page_==MenuPage::error){page_=MenuPage::slots;confirmOverwrite_=false;}else page_=MenuPage::closed;}
 MenuAction confirm(const std::array<bool,3>& occupied){
  if(page_==MenuPage::overwrite){if(!confirmOverwrite_){back();return {};}return {true,true,true,selected_};}
  if(page_!=MenuPage::slots)return {};
  if(fresh_&&occupied[selected_]){page_=MenuPage::overwrite;confirmOverwrite_=false;return {};}
  if(!fresh_&&!occupied[selected_])return {};
  return {true,fresh_,false,selected_};
 }
 void completed(bool success){page_=success?MenuPage::closed:MenuPage::error;}
};
}
