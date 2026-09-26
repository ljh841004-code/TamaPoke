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
    "Looking for WiFi...", "AUTO ON", "AUTO OFF", "OPEN WiFi ON", "OPEN WiFi OFF", " +%u",
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
    "WiFi 찾는 중...", "자동 켬", "자동 끔", "개방 WiFi 켬", "개방 WiFi 끔", " 외 %u개",
  },
};

static const char *const MOVES_EN[PT_COUNT] = {
  "HEADBUTT", "EMBER", "WATER GUN", "VINE WHIP", "THUNDERSHOCK", "ICE BEAM", "KARATE CHOP",
  "POISON STING", "MUD-SLAP", "CONFUSION", "PIN MISSILE", "ROCK THROW", "LICK", "DRAGON RAGE",
};
static const char *const MOVES_KO[PT_COUNT] = {
  "박치기", "불꽃세례", "물대포", "덩굴채찍", "전기쇼크", "냉동빔", "태권당수",
  "독침", "진흙뿌리기", "염동력", "바늘미사일", "돌떨구기", "핥기", "용의분노",
};

static const char *const TYPES_EN[PT_COUNT] = {
  "NORMAL", "FIRE", "WATER", "GRASS", "ELECTRIC", "ICE", "FIGHTING",
  "POISON", "GROUND", "PSYCHIC", "BUG", "ROCK", "GHOST", "DRAGON",
};
static const char *const TYPES_KO[PT_COUNT] = {
  "노말", "불꽃", "물", "풀", "전기", "얼음", "격투",
  "독", "땅", "에스퍼", "벌레", "바위", "고스트", "드래곤",
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

const char *moveName(uint8_t move, uint8_t type) {
  if (move == BA_TYPE && type < PT_COUNT) return isKo() ? MOVES_KO[type] : MOVES_EN[type];
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
