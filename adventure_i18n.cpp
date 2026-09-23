#include "adventure_i18n.h"
#include "i18n.h"
#include <string.h>

// New adventure UI uses Korean when selected; other languages fall back to English.
const char *adventureText(const char *en) {
  if (gLang != LANG_KO) return en;
  struct Pair { const char *en, *ko; };
  static const Pair table[] = {
    {"ADVENTURE", "모험"}, {"WILD BATTLE", "야생 배틀"},
    {"COLLECTION BOX", "보관함"}, {"COLLECTION", "보관함"},
    {"FIGHT", "기술"}, {"POWER", "강공격"}, {"DODGE", "회피"},
    {"POTION", "상처약"}, {"RUN", "도망"}, {"BACK", "뒤로"},
    {"NEXT WAVE", "다음 웨이브"}, {"EXIT", "나가기"},
    {"STORE ACTIVE", "현재 포켓몬 맡기기"},
    {"+3 BALLS", "볼 +3"}, {"+2 POTIONS", "상처약 +2"},
    {"+2 TRAINING", "훈련 +2"},
    {"SEEN - NOT CAUGHT", "발견 / 미포획"},
    {"A wild Pokemon appeared!", "야생 포켓몬 등장!"},
    {"A trainer challenges you!", "트레이너가 승부를 건다!"},
    {"A boss appeared!", "보스 포켓몬 등장!"},
    {"The wild Pokemon dodged!", "상대가 피했다!"},
    {"Dodged! Counter ready.", "회피 성공! 반격 준비"},
    {"No effect!", "효과가 없다!"},
    {"Super effective!", "효과가 굉장했다!"},
    {"Not very effective.", "효과가 별로다."},
    {"A fierce exchange!", "서로 공격했다!"},
    {"Victory! Next wave?", "승리! 보상을 고르세요"},
    {"Your Pokemon fainted.", "포켓몬이 쓰러졌다."},
    {"No PP left!", "PP가 부족하다!"},
    {"The move missed!", "기술이 빗나갔다!"},
    {"Unable to move!", "움직일 수 없다!"},
    {"Status inflicted!", "상태이상 성공!"},
    {"No Poke Balls left.", "볼이 부족하다."},
    {"Cannot catch a trainer Pokemon.", "트레이너 포켓몬은 포획 불가"},
    {"Captured! Sent to the box.", "포획! 보관함에 등록"},
    {"The Pokemon broke free!", "포켓몬이 탈출했다!"},
    {"Got away safely.", "무사히 도망쳤다."},
    {"Could not escape!", "도망치지 못했다!"},
    {"No potions left.", "상처약이 부족하다."},
    {"+3 Poke Balls", "볼 3개 획득"},
    {"+2 Potions", "상처약 2개 획득"},
    {"Training and PP restored!", "훈련 상승 / PP 회복"},
    {"TACKLE", "몸통박치기"}, {"EMBER", "불꽃세례"},
    {"WATER GUN", "물대포"}, {"THUNDER", "전기쇼크"},
    {"VINE WHIP", "덩굴채찍"}, {"ICE BEAM", "냉동빔"},
    {"KARATE CHOP", "태권당수"}, {"POISON STING", "독침"},
    {"MUD SLAP", "진흙뿌리기"}, {"GUST", "바람일으키기"},
    {"CONFUSION", "염동력"}, {"BUG BITE", "벌레먹기"},
    {"ROCK THROW", "돌떨구기"}, {"SHADOW BALL", "섀도볼"},
    {"DRAGON BREATH", "용의숨결"}, {"QUICK HIT", "빠른공격"},
    {"POWER STRIKE", "강타"},
    {"SEEN %u  CAUGHT %u", "%u종 발견 / %u종 등록"},
    {"%u stored / %u caught", "%u마리 보관 / %u종 등록"},
    {"BOSS %u  #%03d", "보스 %u  #%03d"},
    {"TRAINER %u  #%03d", "트레이너 %u  #%03d"},
    {"WAVE %u  #%03d", "%u웨이브  #%03d"},
    {"BALL x%u", "볼 x%u"},
    {"NORMAL", "노말"}, {"FIRE", "불꽃"}, {"WATER", "물"},
    {"ELECTRIC", "전기"}, {"GRASS", "풀"}, {"ICE", "얼음"},
    {"FIGHTING", "격투"}, {"POISON", "독"}, {"GROUND", "땅"},
    {"FLYING", "비행"}, {"PSYCHIC", "에스퍼"}, {"BUG", "벌레"},
    {"ROCK", "바위"}, {"GHOST", "고스트"}, {"DRAGON", "드래곤"}
  };
  for (const auto &item : table) if (strcmp(en, item.en) == 0) return item.ko;
  return en;
}
