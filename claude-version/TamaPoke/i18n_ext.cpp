#include "i18n_ext.h"
#include "dex.h"  // PT_*
#include "battle.h"
#include <string.h>

// [0] = ingles, [1] = coreano. Mantener el orden de XId.
static const char *const XS[2][X_COUNT] = {
  // ---------------- EN ----------------
  {
    "WiFi", "NETWORK", "WiFi not set", "SYNC TIME NOW", "SET UP WIFI", "AUTO SYNC ON", "AUTO SYNC OFF",
    "Connecting...", "Getting time...", "Time synced!", "WiFi failed", "Time server failed", "Radio busy",
    "Last: %02u/%02u %02u:%02u", "Never synced", "On your phone, join WiFi", "then open 192.168.4.1",
    "password: tamapoke", "Saved! Syncing...",
    "TZ", "tap top: close",
    "WILD BATTLE", "LINK 2P", "A wild {1} appeared!", "What will {1} do?",
    "TACKLE", "GUARD", "RUN",
    "{1} used {2}!", "But it missed!", "Super effective!", "Not very effective...", "No effect...",
    "A critical hit!", "{1} is guarding!", "Got away safely!",
    "Can't escape!", "{1} fainted!", "YOU WIN!", "YOU LOST...", "ATK DEF SPD up!", "Too tired to battle",
    "! WILD POKEMON !",
    "Wild %u  Link %u/%u  Trade %u",
    "LINK", "BATTLE", "TRADE", "Searching...", "Open LINK on the other one",
    "Partner: %s Lv.%u", "Trade Pokemon?", "Waiting for partner...", "Trade complete!", "Trade cancelled",
    "Connection lost",
    "Not now (egg / asleep)", "Your Pokemon will leave!", "Partner said YES",
    "POTION %u", "BALL %u", "No potions left", "No Poke Balls left", "You threw a Poke Ball!",
    "Gotcha! {1} was caught!", "Oh no! {1} broke free!", "{1} recovered HP!",
    "Sent to the box", "The box is full", "BALLS +2  POTIONS +2", "GOTCHA!",
    "BOX %u/%u", "BOX", "Empty", "Win or catch in battle", "Next partner comes from here",
    "RELEASE", "CLOSE", "CAUGHT", "WON", "{1} came out of the box!", "BALLS %u  POTIONS %u",
    "Release it?",
    "TRAIN", "TRAINING", "ATTACK: RAPID TAPS", "DEFENSE: FALLING BALLS", "SPEED: QUICK TAP",
    "BALL GAME (JOY)", "BEST %u", "Tap the falling balls!", "Tap the ball when it shows!",
    "BLOCKED %u", "HITS %u/15", "DEF +%u", "SPD +%u", "NICE!", "MISSED",
    "SOUND", "MUSIC", "CRIES", "SYSTEM", "SOUND ON", "SOUND OFF", "DONE",
    "EVOLVED", "COMMON", "RARE", "LEGEND", "HP %u  AT %u  DF %u  SP %u", "EVOLVES: %s Lv%u",
    "FINAL FORM", "FIRST SEEN %02u/%02u", "MET %u  CAUGHT %u", "RAISED BEFORE", "Not discovered yet",
    "It wants to join you!", "SD UPDATE", "SD CARD UPDATE", "No update.bin on the SD card",
    "Use the app file (0x10000)", "update.bin  %u KB", "UPDATE", "CANCEL", "Updating... do not power off",
    "Done! Restarting...", "Update failed", "Send update.bin with the web installer",
    "DOUBLE TAP: EXIT",
    "NOW v%s", "FILE v%s", "Same version already",
    "%lu EXP to next level", "EXP +%lu", "Grew to Lv.%u!", "MAX LEVEL",
    "NEW GAME", "START OVER", "Pokemon, dex, box and records", "will all be erased.",
    "WiFi, sound and language stay", "HOLD 3s", "Keep the button pressed for 3 s", "Starting over...",
    "Scan with phone camera", "WiFi", "PASSWORD", "ADDRESS",
    "SPACE", "DEL", "KOREAN",
    "CANDY", "or ~%luh %lum of raising", "or ~%lu min of raising", "Hold 2 s in a game to quit",
    "JOY + ENERGY UP!", "Beat the record for a bonus", "ENERGY +5 (record = JOY too)",
    "Look for another Pokemon?", "KEEP GOING", "EXIT",
    "Looking for WiFi...", "AUTO ON", "AUTO OFF", "OPEN WiFi ON", "OPEN WiFi OFF", " +%u",
    "%d.%02u.%02u %s", "SUNMONTUEWEDTHUFRISAT",
    "WHERE TO?", "A wild {2} appeared! ({1})", "Wow! A rare {2} appeared! ({1})",
    "You already have {1}!", "BOX (+%u candy)", "%u CANDY", "{1} candy +{2}", "{1} candy: {2}", "CANDY",
    "EXP UP", "JOY/ENERGY/FOOD +20", "STATS UP", "SHINY EGG UP", "EVOLVE 1 LV SOONER", "SHINY UP: ON",
    "Used!", "Can't use now",
    "It's raining! (Water up, Fire down)", "Harsh sunlight! (Fire up, Water down)", "It's snowing! (Ice up)",
    "GYMS", "DAILY", "GYMS  (badges %u/8)", "BATTLE!", "REMATCH", "%u badges needed",
    "Leader {1} wants to battle!", "{1} sent out {2}!", "Not in a trainer battle!", "Got the {1}! ({2}/8)",
    "BROCK", "MISTY", "LT. SURGE", "ERIKA", "KOGA", "SABRINA", "BLAINE", "GIOVANNI",
    "Boulder Badge", "Cascade Badge", "Thunder Badge", "Rainbow Badge", "Soul Badge", "Marsh Badge",
    "Volcano Badge", "Earth Badge",
    "%s (%u badges)", "More badges open this place",
    "3 battles at the {1}", "Prize: balls, potions, candy +3", "Done today! (no prize again)",
    "Set the clock first", "Daily challenge! 3 battles at the {1}", "Daily cleared! Balls, potions, candy +3",
    "Cleared %u times", "The foe",
    "MONTH", "DAY", "Clock set from your friend",
    "Music (bgm.wav): %u:%02u", "Music: no bgm.wav on SD", "x%u",
    "RAISED", "Choose your next partner", "NEW EGG (random)", "Starts as {1} Lv.1",
    "Already raised", "Swipe down: keep the egg",
    "     HALL %u", "Pokemon you raised to the end appear here", "Raised to the end (keepsake, can't be released)",
    "MEADOW", "BEACH", "FOREST", "VOLCANO", "MOUNTAIN", "SNOWFIELD", "POWER PLANT", "DOJO",
    "SWAMP", "DESERT", "RUINS", "GARDEN", "GRAVEYARD", "DRAGON VALE", "CITY", "MINE",
  },
  // ---------------- KO ----------------
  {
    "WiFi", "네트워크", "WiFi 미설정", "지금 시간 맞추기", "WiFi 설정하기", "자동 동기화 켬", "자동 동기화 끔",
    "연결 중...", "시간 받는 중...", "시간 맞춤 완료!", "WiFi 연결 실패", "시간 서버 실패", "무선 사용 중",
    "최근: %02u/%02u %02u:%02u", "동기화 기록 없음", "휴대폰 WiFi에서 연결하고", "192.168.4.1 을 여세요",
    "비밀번호: tamapoke", "저장! 동기화 중...",
    "시간대", "위쪽 탭: 닫기",
    "야생 배틀", "통신 2인", "앗! 야생 {1}{이} 나타났다!", "{1}{은} 무엇을 할까?",
    "몸통박치기", "방어", "도망",
    "{1}의 {2}!", "그러나 빗나갔다!", "효과가 굉장했다!", "효과가 별로인 듯하다...", "효과가 없는 것 같다...",
    "급소에 맞았다!", "{1}{은} 방어 태세!", "무사히 도망쳤다!",
    "도망칠 수 없었다!", "{1}{은} 쓰러졌다!", "승리!", "패배...", "공격 방어 속도 상승!", "너무 지쳐서 못 싸워요",
    "! 야생 포켓몬 출현 !",
    "야생승 %u 통신 %u/%u 교환 %u",
    "통신", "대전", "교환", "상대를 찾는 중...", "다른 기기도 통신을 여세요",
    "상대: %s Lv.%u", "포켓몬을 교환할까요?", "상대를 기다리는 중...", "교환 완료!", "교환 취소",
    "연결이 끊겼어요",
    "지금은 안 돼요 (알/수면)", "지금 포켓몬이 떠나요!", "상대는 수락했어요",
    "물약 %u", "볼 %u", "물약이 없어요", "포켓볼이 없어요", "포켓볼을 던졌다!",
    "신난다! {1}{을} 잡았다!", "앗! {1}{이} 빠져나왔다!", "{1}의 체력이 회복됐다!",
    "보관함으로 보냈어요", "보관함이 가득 찼어요", "포켓볼 +2  물약 +2", "잡았다!",
    "보관함 %u/%u", "보관함", "비어 있어요", "배틀에서 이기거나 잡으면 들어와요", "다음 육성 포켓몬은 여기서 나와요",
    "놓아주기", "닫기", "포획", "승리", "보관함에서 {1}{이} 나왔다!", "포켓볼 %u   물약 %u",
    "놓아줄까요?",
    "훈련", "훈련 선택", "공격: 빠르게 연타", "방어: 떨어지는 볼 막기", "속도: 나타난 볼 터치",
    "공놀이 (기분)", "최고 %u", "떨어지는 볼을 터치!", "볼이 나타나면 터치!",
    "막은 볼 %u개", "성공 %u/15", "방어 +%u", "속도 +%u", "좋아!", "놓쳤다",
    "소리 설정", "배경음", "포켓몬 목소리", "시스템음", "소리 켜짐", "소리 꺼짐", "완료",
    "진화형", "흔함", "희귀", "전설", "HP %u  공 %u  방 %u  속 %u", "진화: %s (Lv.%u)",
    "최종 진화형", "처음 발견 %02u/%02u", "만남 %u  포획 %u", "키운 적 있어요", "아직 발견하지 못했어요",
    "동료가 되고 싶어 해요!", "SD 업데이트", "SD카드로 업데이트", "SD카드에 update.bin이 없어요",
    "app 파일(0x10000용)을 넣어주세요", "update.bin  %u KB", "업데이트", "취소", "업데이트 중... 전원을 끄지 마세요",
    "완료! 다시 시작해요", "업데이트 실패", "웹 설치 페이지로 update.bin을 보내주세요",
    "두 번 탭: 나가기",
    "현재 v%s", "파일 v%s", "이미 같은 버전이에요",
    "다음 레벨까지 %lu EXP", "경험치 +%lu", "레벨 %u 달성!", "최고 레벨!",
    "새로 시작", "처음부터 새로 시작", "포켓몬, 도감, 보관함, 기록이", "모두 지워져요",
    "WiFi, 소리, 언어 설정은 남아요", "3초 누르기", "버튼을 3초 동안 누르고 있으세요", "처음부터 시작해요!",
    "카메라로 QR 찍기", "WiFi", "비밀번호", "주소",
    "띄움", "지움", "한글",
    "사탕", "또는 육성 약 %lu시간 %lu분", "또는 육성 약 %lu분", "게임 중 2초 누르면 그만",
    "기분, 기력 상승!", "최고 기록을 넘으면 기분, 기력 상승", "기력 +5 (기록을 넘으면 기분도 상승)",
    "다른 포켓몬을 계속 만날까요?", "계속 만나기", "나가기",
    "WiFi 찾는 중...", "자동 켬", "자동 끔", "개방 WiFi 켬", "개방 WiFi 끔", " 외 %u개",
    "%d.%02u.%02u (%s)", "일월화수목금토",
    "어디로 갈까?", "{1}에서 야생 {2}{이} 나타났다!", "앗! {1}에서 희귀한 {2}{이} 나타났다!",
    "{1}{은} 이미 있어요!", "보관함 (사탕 %u)", "사탕 %u개로", "{1} 사탕 +{2}", "{1} 사탕 {2}개", "사탕",
    "경험치 올리기", "기분 기력 포만 +20", "능력 편차 올리기", "다음 알 샤이니 UP", "진화 1레벨 빠르게", "샤이니 UP 적용 중",
    "사용했어요!", "지금은 쓸 수 없어요",
    "비가 내린다! (물 강함, 불 약함)", "햇볕이 강하다! (불 강함, 물 약함)", "눈이 내린다! (얼음 강함)",
    "체육관", "오늘의 도전", "체육관 (배지 %u/8)", "도전!", "재도전", "배지 %u개 필요",
    "관장 {1}{이} 승부를 걸어왔다!", "{1}{은} {2}{을} 내보냈다!", "트레이너 배틀에서는 안 돼요!", "{1} 획득! ({2}/8)",
    "웅", "이슬", "마티스", "민화", "독수", "초련", "강연", "비주기",
    "회색배지", "블루배지", "오렌지배지", "무지개배지", "핑크배지", "골드배지", "크림슨배지", "그린배지",
    "%s (배지%u)", "배지를 더 모으면 열려요",
    "{1}에서 3연전", "보상: 포켓볼, 물약, 사탕 +3", "오늘은 완료! (다시 해도 보상 없음)",
    "시계를 먼저 맞춰 주세요", "오늘의 도전! {1}에서 3연전", "오늘의 도전 성공! 포켓볼, 물약, 사탕 +3",
    "지금까지 %u번 성공", "상대",
    "월", "일", "친구 다마포케에서 시간을 받았어요",
    "배경음 (bgm.wav): %u분 %02u초", "배경음: SD에 bgm.wav 없음", "x%u",
    "키움", "다음 파트너를 골라 주세요", "새 알 (무작위)", "{1} Lv.1부터 시작",
    "이미 키운 계열", "아래로 밀면 새 알로 시작",
    "     왕관 %u", "끝까지 키운 포켓몬이 여기에 남아요", "끝까지 키운 포켓몬 (기념, 놓아줄 수 없음)",
    "초원", "바닷가", "숲", "화산", "산", "설원", "발전소", "도장",
    "늪", "사막", "유적", "꽃밭", "묘지", "용의 계곡", "도시", "광산",
  },
};

// ko10.4: tres ataques por tipo segun la fase evolutiva (basico, medio, definitivo)
static const char *const MOVES_EN[3][PT_COUNT] = {
  { "HEADBUTT", "EMBER", "WATER GUN", "VINE WHIP", "THUNDERSHOCK", "ICY WIND", "KARATE CHOP",
    "POISON STING", "MUD-SLAP", "CONFUSION", "PIN MISSILE", "ROCK THROW", "LICK", "TWISTER",
    "BITE", "METAL CLAW" },
  { "BODY SLAM", "FLAMETHROWER", "WATER PULSE", "RAZOR LEAF", "THUNDERBOLT", "ICE BEAM", "CROSS CHOP",
    "SLUDGE", "MAGNITUDE", "PSYBEAM", "FURY CUTTER", "ANCIENTPOWER", "NIGHT SHADE", "DRAGON RAGE",
    "FAINT ATTACK", "STEEL WING" },
  { "HYPER BEAM", "FIRE BLAST", "HYDRO PUMP", "SOLARBEAM", "THUNDER", "BLIZZARD", "DYNAMICPUNCH",
    "SLUDGE BOMB", "EARTHQUAKE", "PSYCHIC", "MEGAHORN", "ROCK SLIDE", "SHADOW BALL", "OUTRAGE",
    "CRUNCH", "IRON TAIL" },
};
static const char *const MOVES_KO[3][PT_COUNT] = {
  { "박치기", "불꽃세례", "물대포", "덩굴채찍", "전기쇼크", "얼다바람", "태권당수",
    "독침", "진흙뿌리기", "염동력", "바늘미사일", "돌떨구기", "핥기", "회오리",
    "물기", "메탈크로우" },
  { "누르기", "화염방사", "물의파동", "잎날가르기", "10만볼트", "냉동빔", "크로스촙",
    "오물공격", "매그니튜드", "환상빔", "연속자르기", "원시의힘", "나이트헤드", "용의분노",
    "속여때리기", "강철날개" },
  { "파괴광선", "불대문자", "하이드로펌프", "솔라빔", "번개", "눈보라", "폭발펀치",
    "오물폭탄", "지진", "사이코키네시스", "메가혼", "스톤샤워", "섀도볼", "역린",
    "깨물어부수기", "아이언테일" },
};

static const char *const TYPES_EN[PT_COUNT] = {
  "NORMAL", "FIRE", "WATER", "GRASS", "ELECTRIC", "ICE", "FIGHTING",
  "POISON", "GROUND", "PSYCHIC", "BUG", "ROCK", "GHOST", "DRAGON",
  "DARK", "STEEL",
};
static const char *const TYPES_KO[PT_COUNT] = {
  "노말", "불꽃", "물", "풀", "전기", "얼음", "격투",
  "독", "땅", "에스퍼", "벌레", "바위", "고스트", "드래곤",
  "악", "강철",
};

static inline bool isKo() { return gLang == LANG_KO; }

const char *typeName(uint8_t type) {
  if (type >= PT_COUNT) type = PT_NORMAL;
  return isKo() ? TYPES_KO[type] : TYPES_EN[type];
}

const char *XT(XId id) {
  if (id >= X_COUNT) return "";
  return XS[isKo() ? 1 : 0][id];
}

const char *moveName(uint8_t move, uint8_t type, uint8_t tier) {
  if (tier > 2) tier = 2;
  if (move == BA_TYPE && type < PT_COUNT) return isKo() ? MOVES_KO[tier][type] : MOVES_EN[tier][type];
  return XT(X_M_TACKLE);
}

// ultima silaba hangul (U+AC00..U+D7A3) del texto ya escrito: -1 si no hay,
// 0 si acaba en vocal, 1 si acaba en consonante (batchim)
static int lastBatchim(const char *s, size_t len) {
  if (len < 3) return -1;
  const unsigned char *p = (const unsigned char *)s + len - 3;
  if ((p[0] & 0xF0) != 0xE0 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return -1;
  uint32_t cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
  if (cp < 0xAC00 || cp > 0xD7A3) return -1;
  return ((cp - 0xAC00) % 28) ? 1 : 0;
}

static void append(char *out, size_t n, size_t &len, const char *s) {
  while (*s && len + 1 < n) {
    // no cortar a mitad de una secuencia UTF-8: si no cabe entera, parar
    unsigned char c = (unsigned char)*s;
    size_t cl = (c < 0x80) ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
    if (len + cl + 1 > n) break;
    for (size_t i = 0; i < cl && *s; i++) out[len++] = *s++;
  }
  out[len] = 0;
}

void txFmtRaw(char *out, size_t n, const char *tpl, const char *a1, const char *a2) {
  if (!out || !n) return;
  size_t len = 0;
  out[0] = 0;
  // particulas: {forma con batchim} -> [con batchim, sin batchim]
  static const char *const JOSA[][3] = {
    { "{은}", "은", "는" }, { "{이}", "이", "가" }, { "{을}", "을", "를" }, { "{와}", "과", "와" },
  };
  for (const char *p = tpl; *p && len + 1 < n;) {
    if (p[0] == '{' && (p[1] == '1' || p[1] == '2') && p[2] == '}') {
      append(out, n, len, (p[1] == '1' ? a1 : a2) ? (p[1] == '1' ? a1 : a2) : "");
      p += 3;
      continue;
    }
    bool done = false;
    for (auto &j : JOSA) {
      size_t tl = strlen(j[0]);
      if (strncmp(p, j[0], tl) == 0) {
        int b = lastBatchim(out, len);
        append(out, n, len, b == 1 ? j[1] : j[2]);  // latino o sin hangul: forma vocal
        p += tl;
        done = true;
        break;
      }
    }
    if (done) continue;
    char one[2] = { *p++, 0 };
    append(out, n, len, one);
  }
}

void txFmt(char *out, size_t n, XId id, const char *a1, const char *a2) {
  txFmtRaw(out, n, XT(id), a1, a2);
}
