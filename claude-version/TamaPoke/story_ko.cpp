// story_ko.cpp - ko11.21: guiones del modo historia (coreano)
// Personajes y lugares de Pokemon Rojo/Azul y del anime (uso personal); los
// dialogos son nuestros, escritos para TamaPoke (no son los del juego ni del anime).
#include "story.h"

#define BG(r)          { ST_BG, 0, r, 0, 0, nullptr }
#define SAY(w, s)      { ST_SAY, w, 0, 0, 0, s }
#define PET(s)         { ST_SAY, W_PET, 0, 0, 0, s }
#define NARR(s)        { ST_NARR, 0, 0, 0, 0, s }
#define MON(d)         { ST_MON, 0, 0, 0, d, nullptr }
#define CHOICE(s, a, b) { ST_CHOICE, 0, a, b, 0, s }
#define LABEL(n)       { ST_LABEL, 0, n, 0, 0, nullptr }
#define GOTO(n)        { ST_GOTO, 0, n, 0, 0, nullptr }
#define BATTLE(t, w)   { ST_BATTLE, w, t, 0, 0, nullptr }
#define GIVE(k, n, d)  { ST_GIVE, 0, k, n, d, nullptr }
#define END()          { ST_END, 0, 0, 0, 0, nullptr }
#define STARTER(d)     { ST_STARTER, 0, 0, 0, d, nullptr }
#define CHOICE3(s, a, b, c) { ST_CHOICE, 0, a, b, c, s }
#define JOIN(d)        { ST_JOIN, 0, 0, 0, d, nullptr }

// regiones: 0 초원 1 바닷가 2 숲 3 화산 4 산 5 설원 6 발전소 7 도장 8 늪 9 사막
//           10 유적 11 꽃밭 12 묘지 13 용의 계곡 14 도시 15 광산
const STeam STORY_TEAMS[] = {
  { 1, { -1 }, { 5 }, 0 },                        // 0 juego 1: 그린 (su inicial)
  { 2, { 74, 95 }, { 12, 14 }, 4 },               // 1 juego 2: 웅
  { 2, { 23, 109 }, { 12, 13 }, 15 },             // 2 juego 3: 로켓단 조무래기 (달맞이산)
  { 2, { 120, 121 }, { 18, 21 }, 1 },             // 3 juego 4: 이슬
  { 3, { 17, 63, -1 }, { 17, 16, 20 }, 0 },       // 4 juego 4: 그린 (너굴다리)
  { 2, { 42, 88 }, { 22, 23 }, 14 },              // 5 juego 5: 조무래기 (게임코너 지하)
  { 3, { 95, 111, 115 }, { 25, 24, 29 }, 14 },    // 6 juego 5: 비주기
  { 2, { 21, 21 }, { 4, 5 }, 0 },                 // 7 anime 1: 깨비참 무리
  { 3, { 23, 109, 52 }, { 8, 8, 9 }, 2 },         // 8 anime 2: 로켓단
  { 2, { 54, 120 }, { 12, 13 }, 1 },              // 9 anime 3: 이슬
  { 2, { 74, 95 }, { 12, 14 }, 4 },               // 10 anime 3: 웅
  { 3, { 23, 109, 52 }, { 14, 14, 15 }, 2 },      // 11 anime 4: 로켓단 (파이리를 노림)
  { 3, { 24, 110, 52 }, { 22, 22, 22 }, 14 },     // 12 anime 5: 로켓단 (진화)
};
const uint8_t STORY_TEAM_COUNT = sizeof(STORY_TEAMS) / sizeof(STORY_TEAMS[0]);

const char *const STORY_WHO_NAME[W_COUNT] = {
  "", "오박사", "그린", "레드", "로켓단 조무래기", "비주기", "웅", "이슬", "지우", "로켓단",
  "마티스", "민화", "독수", "초련", "강연", "시바", "목호",
};
const char *const STORY_STYLE_NAME[3] = { "관동 여행기", "지우와 함께", "원정" };
const char *const STORY_STYLE_SUB[3] = { "게임 스토리 (레드/그린)", "애니메이션 스토리", "포켓로그 방식" };
const uint8_t STORY_FLOOR[STORY_STYLES][STORY_CHAPTERS] = { { 5, 12, 13, 18, 24 }, { 5, 8, 12, 14, 21 } };
const int16_t STORY_PARTNER0[STORY_STYLES] = { 4, 25 };

// ======================= juego (Rojo/Azul) =======================
static const SStep G1[] = {
  BG(0),
  NARR("태초마을. 오늘은 포켓몬 트레이너로 떠나는 날이다."),
  SAY(W_OAK, "오, 왔구나! 기다리고 있었단다."),
  SAY(W_OAK, "여기 몬스터볼 세 개에 포켓몬이 한 마리씩 들어 있단다."),
  SAY(W_OAK, "너와 함께 여행할 첫 포켓몬이다. 마음에 드는 녀석을 골라 보렴."),
  CHOICE3("어느 포켓몬을 고를까?|이상해씨|파이리|꼬부기", 11, 12, 13),
  LABEL(11),
  STARTER(1),
  MON(1),
  SAY(W_OAK, "풀 포켓몬 이상해씨로구나. 듬직한 선택이다!"),
  GOTO(14),
  LABEL(12),
  STARTER(4),
  MON(4),
  SAY(W_OAK, "불꽃 포켓몬 파이리로구나. 꼬리의 불꽃이 힘차구나!"),
  GOTO(14),
  LABEL(13),
  STARTER(7),
  MON(7),
  SAY(W_OAK, "물 포켓몬 꼬부기로구나. 씩씩한 녀석이지!"),
  LABEL(14),
  MON(0),
  PET("{1}{이} 기쁜 듯이 다가온다!"),
  SAY(W_RIVAL, "할아버지! 나도 고를래요. 흥, 그럼 난... 이 녀석으로 하지!"),
  SAY(W_RIVAL, "누가 더 센지 지금 바로 확인해 보자고!"),
  SAY(W_OAK, "허허, 그린. 첫 승부라... 좋다, 둘 다 힘내거라!"),
  BATTLE(0, W_RIVAL),
  SAY(W_RIVAL, "쳇... 이번엔 운이 좋았던 거야. 다음엔 안 봐줘!"),
  SAY(W_OAK, "훌륭했다. 그럼 부탁이 하나 있단다."),
  SAY(W_OAK, "관동 지방의 모든 포켓몬을 기록하는 도감을 완성해 주겠니?"),
  CHOICE("오박사의 부탁을 받아들일까?|맡겨 주세요!|조금 무섭지만... 해 볼게요", 1, 2),
  LABEL(1),
  SAY(W_OAK, "하하! 그 기세, 마음에 드는구나."),
  GOTO(3),
  LABEL(2),
  SAY(W_OAK, "처음엔 누구나 그렇단다. {1}{이} 곁에 있잖니."),
  LABEL(3),
  SAY(W_OAK, "여행에 쓰거라. 몬스터볼과 상처약이란다."),
  GIVE(SG_BALL, 5, 0),
  GIVE(SG_POTION, 3, 0),
  NARR("이렇게 {1}{와} 함께하는 여행이 시작되었다!"),
  END(),
};

static const SStep G2[] = {
  BG(2),
  NARR("상록숲. 나무 사이로 벌레 포켓몬들의 소리가 들린다."),
  PET("{1}{이} 수풀 쪽을 가만히 바라본다."),
  MON(16),
  NARR("수풀에서 구구 한 마리가 튀어나왔다! 몇 번 겨루고 나니 순순히 몬스터볼에 들어왔다."),
  JOIN(16),
  MON(0),
  NARR("숲을 빠져나오자 회색시티의 체육관이 보였다."),
  BG(4),
  SAY(W_BROCK, "나는 회색시티 체육관 관장 웅. 단단한 바위 같은 의지가 내 신조다!"),
  SAY(W_BROCK, "바위 포켓몬의 단단함, 네 포켓몬이 버틸 수 있을까?"),
  BATTLE(1, W_BROCK),
  SAY(W_BROCK, "훌륭하다! 너와 {1}의 호흡은 바위도 깨는구나."),
  SAY(W_BROCK, "이건 내가 주는 선물이다. 앞으로의 여행에 보탬이 되길."),
  GIVE(SG_CANDY, 3, 0),
  PET("{1}{이} 자랑스러운 듯이 가슴을 편다!"),
  END(),
};

static const SStep G3[] = {
  BG(15),
  NARR("달맞이산. 어두운 동굴 속에서 발소리가 울린다."),
  SAY(W_GRUNT, "거기 서! 이 산의 화석은 전부 로켓단이 가져간다!"),
  PET("{1}{이} 날카롭게 경계한다!"),
  SAY(W_GRUNT, "방해하면 가만두지 않겠어. 가라!"),
  BATTLE(2, W_GRUNT),
  SAY(W_GRUNT, "크윽... 이런 꼬마한테 지다니! 기억해 두겠어!"),
  NARR("조무래기가 도망치며 화석 두 개를 떨어뜨렸다."),
  CHOICE("어느 화석을 가져갈까?|조개 모양 화석|껍질 모양 화석", 1, 2),
  LABEL(1),
  MON(138),
  NARR("조개 화석에서 옛 포켓몬 암나이트의 기운이 느껴진다."),
  GIVE(SG_CANDY, 3, 138),
  GOTO(3),
  LABEL(2),
  MON(140),
  NARR("껍질 화석에서 옛 포켓몬 투구의 기운이 느껴진다."),
  GIVE(SG_CANDY, 3, 140),
  LABEL(3),
  MON(0),
  NARR("남은 화석은 연구소로 보내기로 했다."),
  NARR("동굴을 나서려는데, 달빛 아래 작은 포켓몬이 따라온다."),
  MON(35),
  PET("{1}{이} 반갑게 다가가자 피피가 빙글 돌며 웃었다."),
  JOIN(35),
  MON(0),
  END(),
};

static const SStep G4[] = {
  BG(1),
  NARR("블루시티. 푸른 바다가 보이는 체육관에 도착했다."),
  SAY(W_MISTY, "어서 와! 나는 이슬. 물 포켓몬의 아름다움을 보여 줄게!"),
  BATTLE(3, W_MISTY),
  SAY(W_MISTY, "아이참, 졌다! 하지만 {1}, 정말 멋졌어."),
  BG(0),
  NARR("너굴다리 위에서 낯익은 목소리가 들린다."),
  SAY(W_RIVAL, "야! 너 아직도 여기서 어슬렁거리냐?"),
  SAY(W_RIVAL, "난 벌써 포켓몬을 잔뜩 모았다고. 실력 차이를 보여 주지!"),
  BATTLE(4, W_RIVAL),
  SAY(W_RIVAL, "뭐, 뭐야... 너, 제법 강해졌잖아."),
  SAY(W_RIVAL, "흥, 다음 도시에서 두고 보자. 먼저 간다!"),
  GIVE(SG_POTION, 2, 0),
  GIVE(SG_EXP, 30, 0),
  END(),
};

static const SStep G5[] = {
  BG(14),
  NARR("무지개시티의 게임코너. 포스터 뒤에 수상한 스위치가 있다."),
  PET("{1}{이} 포스터 쪽으로 코를 킁킁거린다."),
  NARR("스위치를 누르자 지하로 내려가는 계단이 나타났다!"),
  SAY(W_GRUNT, "여긴 로켓단 아지트다! 어떻게 들어온 거냐?"),
  BATTLE(5, W_GRUNT),
  SAY(W_GRUNT, "보, 보스! 침입자예요!"),
  SAY(W_GIO, "소란스럽군. 여기까지 찾아온 꼬마가 너인가."),
  SAY(W_GIO, "나는 로켓단의 보스, 비주기. 포켓몬은 돈을 벌기 위한 도구일 뿐이다."),
  PET("{1}{이} 화난 듯이 앞으로 나선다!"),
  CHOICE("비주기에게 뭐라고 할까?|포켓몬은 도구가 아니야!|{1}{와} 함께라면 안 져!", 1, 1),
  LABEL(1),
  SAY(W_GIO, "흥... 그 말이 진짜인지 보여 봐라."),
  BATTLE(6, W_GIO),
  SAY(W_GIO, "...좋다. 이번에는 물러나 주지."),
  SAY(W_GIO, "하지만 로켓단은 사라지지 않는다. 또 만나게 되겠지."),
  NARR("비주기는 어둠 속으로 사라졌다. 여행은 계속된다..."),
  GIVE(SG_RARE, 1, 0),
  GIVE(SG_BALL, 5, 0),
  END(),
};

// ======================= anime =======================
static const SStep A1[] = {
  BG(0),
  NARR("태초마을의 아침. 알람 시계는 바닥에 굴러다니고 있었다..."),
  SAY(W_ASH, "으악, 늦잠 잤다! 오늘 오박사님께 첫 포켓몬을 받는 날인데!"),
  SAY(W_OAK, "지우, 이제야 왔구나. 이상해씨, 파이리, 꼬부기는 벌써 다른 아이들이 데려갔단다."),
  SAY(W_ASH, "네에?! 그럼 저는요...?"),
  SAY(W_OAK, "음... 한 마리 남아 있긴 하단다. 조금 까다로운 녀석이지만."),
  STARTER(25),
  MON(25),
  NARR("몬스터볼에서 노란 포켓몬이 튀어나왔다. 피카츄다!"),
  PET("{1}{이} 볼에 들어가기 싫다는 듯 고개를 홱 돌린다."),
  SAY(W_ASH, "잘 부탁해, 피카츄! 우리 같이 포켓몬 마스터가 되자!"),
  MON(0),
  NARR("길을 걷다 풀숲에 돌을 던진 순간... 깨비참 무리가 날아올랐다!"),
  SAY(W_ASH, "으악! 화났나 봐! 피카츄, 부탁해!"),
  BATTLE(7, W_NONE),
  SAY(W_ASH, "피카츄... 나를 지켜 준 거야? 고마워!"),
  PET("{1}{이} 처음으로 지우의 어깨 위에 올라탔다."),
  NARR("비구름이 걷히고, 하늘에 무지개가 떴다."),
  SAY(W_ASH, "봐, 무지개 너머로 뭔가 날아가! 우리 여행, 분명 신날 거야!"),
  GIVE(SG_POTION, 2, 0),
  END(),
};

static const SStep A2[] = {
  BG(2),
  NARR("상록숲. 나뭇가지에서 캐터피 한 마리가 지우를 빤히 쳐다본다."),
  MON(10),
  SAY(W_ASH, "좋아, 내 첫 번째 포획이다! 가라, 몬스터볼!"),
  NARR("딸깍! 캐터피를 잡았다. 캐터피는 언젠가 멋지게 날고 싶다고 한다."),
  JOIN(10),
  MON(0),
  NARR("모두 쉬고 있는데, 어디선가 요란한 목소리가 들린다."),
  SAY(W_TR, "바람을 가르고 나타난 악의 트리오를 모른다고?"),
  SAY(W_TR, "로사! 로이! 그리고 나옹이다옹! 그게 바로 우리 로켓단!"),
  SAY(W_TR, "그 피카츄, 보스께 선물로 딱이다옹! 잡아가자!"),
  SAY(W_ASH, "안 돼! 피카츄는 절대 못 줘!"),
  PET("{1}{이} 지우 앞으로 뛰어나간다!"),
  BATTLE(8, W_TR),
  SAY(W_TR, "으아아~ 이번에도 하늘 끝까지 날아간다~!"),
  NARR("반짝! 로켓단은 하늘 저편으로 사라졌다."),
  SAY(W_ASH, "하하, 이상한 녀석들이네. 잘했어, 피카츄!"),
  GIVE(SG_BALL, 3, 0),
  END(),
};

static const SStep A3[] = {
  BG(1),
  NARR("강가에서 낚시를 하던 소녀가 화가 난 얼굴로 다가온다."),
  SAY(W_MISTY, "너희! 내 자전거를 망가뜨린 게 누구야?"),
  SAY(W_ASH, "그, 그건 사고였어! 미안해!"),
  SAY(W_MISTY, "흥, 그럼 포켓몬 승부로 기분 풀게 해 줘!"),
  BATTLE(9, W_MISTY),
  SAY(W_MISTY, "제법인데? 좋아, 자전거 값 받을 때까지 따라다닐 거야!"),
  BG(4),
  SAY(W_BROCK, "여행자들인가. 나는 웅, 포켓몬 브리더를 꿈꾸고 있지."),
  SAY(W_BROCK, "그 피카츄, 관리가 아주 잘 되어 있군. 한번 겨뤄 볼까?"),
  BATTLE(10, W_BROCK),
  SAY(W_BROCK, "좋은 승부였다! 나도 너희 여행에 함께하지. 요리는 내게 맡겨라!"),
  NARR("이렇게 이슬과 웅이 여행의 동료가 되었다."),
  GIVE(SG_CANDY, 3, 0),
  END(),
};

static const SStep A4[] = {
  BG(2),
  NARR("비가 쏟아지는 숲. 바위 위에 파이리 한 마리가 홀로 앉아 있다."),
  MON(4),
  SAY(W_BROCK, "꼬리 불꽃이 약해지고 있어! 이대로면 위험해."),
  SAY(W_MISTY, "트레이너가 여기서 기다리라고 하고 떠났대... 너무해."),
  CHOICE("파이리를 어떻게 할까?|비를 막아 주자|포켓몬센터로 데려가자", 1, 2),
  LABEL(1),
  NARR("우산을 씌워 주자 파이리의 불꽃이 조금씩 되살아났다."),
  GOTO(3),
  LABEL(2),
  NARR("모두 함께 파이리를 안고 포켓몬센터까지 달렸다."),
  LABEL(3),
  PET("{1}{이} 파이리 곁에 꼭 붙어 체온을 나눈다."),
  SAY(W_TR, "그 파이리, 불꽃이 멋지다옹! 우리가 데려가겠다옹!"),
  SAY(W_ASH, "또 너희냐! 파이리는 우리가 지킨다!"),
  BATTLE(11, W_TR),
  SAY(W_TR, "비 오는 날에도 날아간다옹~!"),
  NARR("파이리가 고마운 듯 작게 불꽃을 피웠다."),
  SAY(W_ASH, "파이리, 우리랑 같이 갈래?"),
  NARR("파이리가 힘차게 고개를 끄덕였다!"),
  JOIN(4),
  GIVE(SG_CANDY, 3, 4),
  MON(0),
  END(),
};

static const SStep A5[] = {
  BG(14),
  NARR("도시의 포켓몬센터. 모두가 잠든 밤, 전등이 꺼졌다!"),
  SAY(W_TR, "이번엔 포켓몬센터의 포켓몬을 몽땅 가져가겠다!"),
  SAY(W_TR, "특훈을 한 우리 포켓몬은 이제 진화했다옹!"),
  SAY(W_ASH, "모두 일어나! 센터의 포켓몬들을 지키자!"),
  PET("{1}{이} 어둠 속에서도 용감하게 앞으로 나선다!"),
  BATTLE(12, W_TR),
  SAY(W_TR, "말도 안 돼... 오늘 밤도 별이 되어 날아간다~!"),
  SAY(W_MISTY, "해냈다! {1}, 정말 멋졌어!"),
  SAY(W_BROCK, "간호순 누나가 고맙다고 선물을 주셨어."),
  SAY(W_ASH, "우리 여행은 이제 시작이야. 다음 도시도 같이 가자!"),
  GIVE(SG_RARE, 1, 0),
  GIVE(SG_POTION, 3, 0),
  END(),
};

#define CH(t, s) { t, s, sizeof(s) / sizeof(s[0]) }
const SChapter STORY[STORY_STYLES][STORY_CHAPTERS] = {
  { CH("1장 태초마을의 출발", G1), CH("2장 상록숲과 회색시티", G2), CH("3장 달맞이산의 로켓단", G3),
    CH("4장 블루시티와 그린", G4), CH("5장 게임코너의 비밀", G5) },
  { CH("1화 너로 정했어!", A1), CH("2화 로켓단 등장", A2), CH("3화 이슬과 웅", A3),
    CH("4화 비 오는 날의 파이리", A4), CH("5화 포켓몬센터 대소동", A5) },
};

const char *const STX[SX_COUNT] = {
  "스토리", "완료", "잠김", "이어하기", "%u/5 완료", "최고 %u웨이브", "{1}{와}의 승부!", "야생 포켓몬 무리가 나타났다!",
  "탭해서 승부 시작", "{1}{이} 지쳐 있어요. 쉬었다가 다시 와요 (진행은 저장돼요)", "졌다... 탭하면 다시 도전",
  "{1} 클리어!", "몬스터볼 %u개를 받았다!", "상처약 %u개를 받았다!", "{1} 사탕 %u개를 받았다!",
  "이상한사탕을 받았다!", "경험치 +%u!", "{1}{이} 승부를 걸어왔다!", "탭해서 계속", "새 이야기", "{1}{이} 동료가 되었다!", "처음부터", "한 번 더: 지우기", "처음부터 다시 시작해요",
};

const char *const RGX[RX_COUNT] = {
  "최고 기록 %u웨이브", "원정 시작", "이어하기", "그만두기", "웨이브 %u", "다음 웨이브", "보상을 하나 고르세요",
  "회복 +40%", "몬스터볼 +1", "사탕 +2", "원정 끝!", "%u웨이브 돌파  경험치 +%u  사탕 +%u", "최고 기록!",
  "{1}{이} 지쳤어요. 쉬었다가 이어해요 (원정은 저장돼요)", "야생 {1}{이} 나타났다!", "원정 트레이너가 승부를 걸어왔다!",
  "{1}의 보스가 나타났다!", "지역이 5웨이브마다 바뀌어요. 5웨이브마다 트레이너, 10웨이브마다 보스!",
  "웨이브 %u 돌파!", "이번 원정을 함께할 포켓몬을 고르세요",
};
