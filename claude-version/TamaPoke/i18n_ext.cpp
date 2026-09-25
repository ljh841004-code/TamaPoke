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

static inline bool isKo() { return gLang == LANG_KO; }

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
