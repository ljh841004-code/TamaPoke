#include "i18n_ext.h"
#include "moves_data.h"
#include "dex.h"  // PT_*
#include "battle.h"
#include <string.h>

// [0] = ingles, [1] = coreano. Mantener el orden de XId.
static const char *const XS[2][X_COUNT] = {
  // ---------------- EN ----------------
  {
    "NETWORK", "WiFi not set", "SYNC TIME NOW", "SET UP WIFI", 
    "Connecting...", "Getting time...", "Time synced!", "WiFi failed", "Time server failed", "Radio busy",
    "Last: %02u/%02u %02u:%02u", "Never synced", 
    "TZ", "tap top: close",
    "WILD BATTLE", "LINK 2P", "What will {1} do?",
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
    "Sent to the box", "The box is full", "GOTCHA!",
    "BOX %u/%u", "Empty", "Win or catch in battle", "Next partner comes from here",
    "RELEASE", "CLOSE", "CAUGHT", "WON", "{1} came out of the box!", "BALLS %u  POTIONS %u",
    "Release it?",
    "TRAINING", "ATTACK: TIMING PUNCH", "DEFENSE: TIMING BLOCK", "SPEED: TAP IN ORDER",
    "TILT BERRIES (JOY)", "Tap when the ball hits green!", "Tap 1, 2, 3... in order!",
    "SCORE %u", "DEF +%u", "SPD +%u", "NICE!", "MISSED",
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
    "You already have {1}!", "BOX (+%u candy)", "%u CANDY", "{1} candy +{2}", "{1} candy: {2}", 
    "EXP UP", "JOY/ENERGY/FOOD +20", "STATS UP", "SHINY EGG UP", "UNDO 1 MISTAKE", "SHINY UP: ON",
    "Used!", "Can't use now",
    "It's raining! (Water up, Fire down)", "Harsh sunlight! (Fire up, Water down)", "It's snowing! (Ice up)",
    "GYMS", "DAILY", "GYMS  (badges %u/8)", "BATTLE!", "%u badges needed",
    "Leader {1} wants to battle!", "{1} sent out {2}!", "Not in a trainer battle!", "Got the {1}! ({2}/8)",
    "BROCK", "MISTY", "LT. SURGE", "ERIKA", "KOGA", "SABRINA", "BLAINE", "GIOVANNI",
    "Boulder Badge", "Cascade Badge", "Thunder Badge", "Rainbow Badge", "Soul Badge", "Marsh Badge",
    "Volcano Badge", "Earth Badge",
    "%s (%u badges)", "More badges open this place",
    "3 battles at the {1}", "Prize: ball+2 potion+2 candy+3", "Done today! (no prize again)",
    "Set the clock first", "Daily challenge! 3 battles at the {1}", "Daily cleared! Ball+2 Potion+2 Candy+3",
    "Cleared %u times", "The foe",
    "MONTH", "DAY", "Clock set from your friend",
    "x%u",
    "RAISED", "Choose your next partner", "NEW EGG (random)", "Starts as {1} Lv.1",
    "Already raised", "Swipe down: keep the egg",
    "     HALL %u", "Pokemon you raised to the end appear here", "Raised to the end (keepsake, can't be released)",
    "Mistake -1 in %luh %lum", "Mistake -1 in %lum", "All stats 40+ for 12h: mistake -1",
    "%u PTS", "HITS %u/%u  AVG %u.%02us",
    "EXP +%lu", "EXP +%lu  CANDY +1", "CANDY +1", "%u DEFEATED", "MOVES %u  DAMAGE %u",
    "Bored...", "Hungry", "Wash me", "Brr",
    "(tap: why)", "No record (older mistake)", "Food ran too low", "Too bored (joy)", "Out of energy",
    "Too dirty", "Last: %s %s",
    "Rematch: %u wins (reward today)", "Rematch: %u wins (done today)", "Rematch reward! Candy+2 Ball+1 Potion+1",
    "Reward already taken today (%u wins)", "%s  %s  ~Lv%u",
    "CHALLENGE THE LEAGUE!", "Get all 8 badges to challenge", "Champion %u times",
    "HALL OF FAME", "Become champion to be listed here", "The Champion",
    "The League! Beat 6 in a row!", "CHAMPION x%u! Candy+10 Ball+3 Potion+3", "In box: %u",
    "Got a RARE CANDY!", "CANDY BAG", "CANDY BAG >", "Rare candy x%u", "%s x%u", "NOW", "No candy yet",
    "3 of these -> 1 of yours", "TRADE 1", "ALL (%u)", "GO USE", "1 -> 5 of yours", "USE", "CLOSE",
    "Done!", "Not while it is an egg", "This is the family you raise", "%u/%u",
    "GEN 1 (1-151)", "GEN 2 (152-251)",
    "HALL OF FAME (%u)", "Latest: %s", "Champion #%u", "%u.%02u.%02u", "Genes  ATK %u DEF %u SPD %u", "Tap: back to list",
    "BALL +1", "POTION +1", "draw avg %u max %ums  stall max %ums", 
    "SAVE BACKUP", "SAVE BACKUP (SD)", "BACK UP NOW", "empty", "latest", "No SD card", "Backed up!", "Backup failed",
    "Backup found on the SD card", "RESTORE", "CANCEL", "Restore this backup?", "Current progress will be lost",
    "Tap a backup to restore", "Auto: daily + evolve/champion/farewell", "Restoring... restarting",
    "Streak %u  (best %u)", "Streak broken (%u in a row)", "%u in a row", "MANUAL", "AUTO",
    "Today: more %s Pokemon", "Weekend: shiny x2", "Full moon night: legends x3!",
    "Friendship %u/%u needed",
    "EXPLORE", "AWAY", "How long to explore?", "%u h", "Exploring: %s  %u:%02u left", "%s is back! Tap to collect",
    "%s: expedition", "%s candy +%u", "Ball +%u  Potion +%u", "Rare candy +1!", "Brought a new friend: %s", "The box is full",
    "%s is back from exploring (box)", "%s left to explore", "Off to %s, its home", "Tap to close",
    "Dex %u caught! %s", "Ball +5", "Potion +5", "Rare candy +1", "Rare candy +3", "Rare candy +5, shiny charm", "Rare candy +5",
    "Rare candy +10, shiny charm", "Caught %u/251  next prize at %u", "Caught %u/251  all prizes done",
    "Background music >", "Background music", "Only the songs that are ON play, at random", "Song %u", "Keep at least one song ON",
    "Add bgm%u.wav ... bgm8.wav to the SD card for more", "ON", "OFF",
    "{1} wants to come along!", "Take it with you?", "Take", "Let go", "{1} went back home", "{1} joined the box!", "{1} strikes back!",
    "Volleyball: first to 5", "First to 5 points wins!", "Your Pokemon runs to the ball by itself",
    "Tap = jump, hit it in the air = SPIKE!", "Point!", "Their point...", "You win!", "So close...",
    "%u wins in a row (best %u)", "Speed +%u", "SPIKE!", "Tap to close", "Hold the score 2 s = quit",
    "Reboot log (%u) >", "Reboot log", "Last: %s", "Cause: %s", "Where: %s %u.%u", "Times: %u",
    "The program crashed (a bug)", "It froze and restarted itself", "Power dropped too low",
    "Full log on the SD: tpsave/crash.txt", "Clear", "Close",
    "Best %u / all-time %u", "ALL-TIME RECORD!",
    "Too young: not kept as raised", "Now %u wins / best %u / all-time %u", "%s POWER SPIKE!",
    "PERFECT!", "GOOD", "TOO EARLY", "TOO LATE", "%u COMBO", "PERFECT %u  GOOD %u",
    "Mash to charge your move!", "WRONG ORDER",
    "SHARDS %u/%u", "LEFTOVERS -> SHARDS", "%u candies -> shards", "No leftover candy",
    "%s ATK ORB", "%s DEF ORB", "ATK +%u%%", "DEF +%u%%", "EQUIP", "REMOVE",
    "EQUIPPED", "Only for %s types", "No orbs yet", "Wild wins, gyms and trips may drop orbs",
    "Got %s +%u%%!", "%s +%u%%! Old one: 1 candy", "%s +%u%%: weaker, 1 candy",
    "Orb didn't fit the new type: 3 candies", "Orb didn't fit the new type: 1 rare candy", "%u/%u",
    "ART: DOT", "ART: GAME",
    "BRIGHTNESS", 
    "HP +%u restored!",
    "{1} slept well!",
    "FUSE ORBS", "FUSE %u/3", "CANCEL", "Fused!", "GREAT FUSION!", "Fusion failed...", "Shards +2 (consolation)", "Not while it is an egg", "You need 3 orbs", "equipped", "Tap to close",
    "AUTO", "x%u", "AUTO battle  %u left", "Auto battle stopped", "Tap the screen to stop",
    "Helpers %u/%u", "Foe: %s type", "%u foes (seen one by one)", "Strong", "Weak", "Solo", "Start %u/%u",
    "Who goes next?", "Next: {1}! Switch?", "Stay", "Switch to? (uses a turn)", "Go, {1}!", "Helpers Lv+1 (%u)",
    "SOLO", "TEAM", "today %u/%u",
    "SOLO+TEAM", "Solo %u / Team %u", "Gold crown now!",
    "PERFECT", "Perfect raising (8/8 medals)",
    "Charge limit", "100%", "~90% (longer life)", 
    "Vibe HIGH", "Vibe OFF", "Vibe MID", "Vibe LOW",
    "SETTINGS", "TIME & DATE", "SOUND & VIBE", "SCREEN & BATTERY", "LANGUAGE %s >", 
    "Favorite food", "Fills more, raises joy and bond", "Try different foods to find out",
    "Day %lu together", "(since %04u.%02u.%02u)",
    "Learned {1}!",
    "FIGHT", "{1} wants to learn {2}!", "Only 4 moves. Forget which?", "NEW: {1}", "DON'T LEARN", "No moves left!",
    "ATTACK", "DEFENSE", "SPEED", "{1}'s {2} rose!", "{1}'s {2} rose a lot!", "{1}'s {2} fell!", "{1}'s {2} fell a lot!", "{1}'s {2} won't go higher", "{1}'s {2} won't go lower",
    "{1} was poisoned!", "{1} was burned!", "{1} is paralyzed!", "{1} fell asleep!", "{1} was frozen!", "{1} became confused!",
    "PSN", "BRN", "PAR", "SLP", "FRZ", "CNF",
    "But it didn't work!", "{1} is hurt by poison!", "{1} is hurt by its burn!", "{1} is too numb to move!", "{1} is fast asleep", "{1} is frozen solid!", "{1} flinched!", "{1} must recharge after that big move!",
    "{1} woke up!", "{1} thawed out!", "{1} snapped out of it!", "{1}'s poison wore off!", "{1}'s burn healed!", "{1} can move freely again!", "{1} hurt itself in confusion!", "{1} is hit by recoil!", "{1} drained energy!", "No moves left!",
    "POKE CENTER", "HEAL", "Healing... %u s", "PP restored!", "Healing now", "PP is full", 
    "POW %u  ACC %u  PP %u", "STATUS MOVE", 
    "{1} left happily!", "Poke Ball +1", "Potion +1", "Shard +1", "EXP +{2}",
    "FAREWELL", "Final form", "3 days together", "Evolve", "%ud %uh", "%uh left to 3 days", "Ready! Farewell button on the main screen", "Evolve to say goodbye",
    "MEADOW", "BEACH", "FOREST", "VOLCANO", "MOUNTAIN", "SNOWFIELD", "POWER PLANT", "DOJO",
    "SWAMP", "DESERT", "RUINS", "GARDEN", "GRAVEYARD", "DRAGON VALE", "CITY", "MINE",
    "%s Rainbow Orb", "ATK/DEF +%u%%", "RAINBOW ORB!",
    "Rainbow orb didn't fit the new type: 10 candies", "Rainbow orb didn't fit the new type: 10 candies + 1 rare candy",
    "MEMORIES", "Memories of %s", "Raised before memories were kept", "(they start with v1.17-ko12.4)", "%u days together",
    "Hatched from an egg", "Came from the box", "Arrived by trade", "Diary started with the update",
    "Evolved: %s  %s", "First win %s   Wild wins %u", "No wins yet", "Said goodbye at Lv.%u  %s", "Nickname: %s", "RECORDS",
    "Meals %u   Snacks %u   Cleanups %u", "Pets %u   Play %u   Training %u", "Link wins %u   Daily %u",
    "Badges +%u   Champion %u", "Bond %u   Mistakes %u   Medals %u", "Ball %u   Sack %u   Guard %u", "Speed %u   Volley %u in a row",
    "Tap: next page",
    "Cushion", "Plant", "Beach ball", "Lamp", "Trophy", "Teddy bear",
    "From the start", "7-day care streak", "Ball game 20 pts", "10k steps/50 wins", "Champion once", "Pokedex 100",
    "DECORATE", "Outdoors", "Room", "Left", "Front", "Right", "Pick an item, then a spot",
    "Now tap a spot", "DECORATE", "WALK", "TODAY'S WALK", "No motion sensor found", "(this board has no QMI8658)",
    "%s steps", "%u goal: %u to go", "Today's goals reached!", "Last 7 days (today on the right)",
    "2,000  Joy +20, Bond +2", "5,000  1 candy", "10,000  2 rare-candy shards",
    "2,000 steps! Joy +20", "5,000 steps! Candy +1", "10,000 steps! Shards +2", "Total %lu steps",
    "Shake: wake up / cheer", "Shook awake!", "Wheee! It loved the shake",
    "Choose the background", "Change it any time: Settings > Decorate",
    "It's throwing a tantrum!", "It doesn't need anything: just wants attention", "SCOLD", "SPOIL",
    "Scolding a tantrum teaches it", "It calmed down. Discipline up!", "It got its way... discipline down", "Tantrum!",
    "Breakfast done! (routine)", "Afternoon play done! (routine)", "Bedtime done! (routine)", "Perfect day! Routine streak %u",
    "Breakfast  6-11 h", "Play  12-19 h", "Bedtime (lights off)  20-24 h",
    "Not clear yet", "Foodie", "Playful", "Cuddly", "Hard worker", "Neat", "Calm",
    "Shows after more care", "Meals fill a bit more", "Playing cheers it up more", "Petting cheers it up more",
    "Training gives more EXP", "Cleaning cheers it up", "Rarely throws tantrums",
    "LIFE", "Personality: %s", "Discipline", "Throwing a tantrum now: tap it", "Today's routine", "Routine streak %u days (best %u)",
    "Cold left untreated", "Has a cold...", "Achoo! It caught a cold. Tap it for medicine", "It walked in the rain and caught a cold!", "The cold went untreated too long (mistake +1)", "It has a cold", "Medicine needed: %u more", "Medicine", "Later", "Dirt, mess or hunger can bring a cold", "Next dose in %u min", "Gulp! One more dose in 3 min", "All better!", "{1} came to visit!", "Played with {1}! Joy +10", "{1} went back to the box", "Birthday", "My birthday", "Not set", "%u/%u", "That day your Pokemon celebrates with fireworks", "Clear", "Save", "Happy birthday!", "{1} is singing for you!", "Gift: joy +30, 1 rare candy", "Tap to close", "Birthday saved!", "Month %u", "Day %u", "Medicine time! Tap your Pokemon",
    "Congratulations! You raised every Pokemon", "Congratulations! Every one, shining", "%u families raised", "Everyone you raised", "No. %u", "Our journey", "Journey began  %04u.%02u.%02u", "Days together  %lu", "Hall of Fame  %u", "Pokedex  %u / 251", "Shiny friends  %u", "Steps walked  %s", "Wild wins  %u", "Champion  %u times", "Link wins %u  Trades %u", "Badges  %u / 8", "And... thank you.", "From the day we met until today, thank you.", "Now let's start a shining journey together!", "Every shining moment with you, I'll never forget.", "You are a true Pokemon Master.", "Title: Pokemon Master / Shiny odds x2 from now", "Title: Shining Pokemon Master", "Tap to continue", "Tap: next", "Ending", "Choose an egg (%u)", "Choose your next egg", "Families not yet raised shiny", "The egg will be {1}!", "Nothing left to choose", "Journey %u/%u families", "Shining journey %u/%u",
    "Pack SD files", "Pack SD files", "%u files / %u MB", "Makes one encrypted file: mons.pak", "Copy only mons.pak to the other SD", "Start", "In use: mons.pak (%u files)", "mons.pak: wrong passphrase", "mons.pak is damaged", "Packing... keep it on", "Done! mons.pak is ready", "Not enough free space on the SD", "Failed (SD error)", "No files in the mons folder", "%u / %u files", "No mons.pak yet (loose files)",
    "It guarded: damage halved!", "Super effective, but guarded: halved!", "Not very effective, and guarded...", "That move must rest next turn", "Resting",
    "Check SD files", "SD files", "Pokemon pictures", "Battle pictures", "Move effects", "Cries", "Small pictures", "Story people", "Music (optional)", "Everything is there!", "Missing: %s (+%u more)", "No SD card", "In mons.pak: %u files",
    "%u candy",
    "Merge with the same Pokemon", "Pick the card to merge", "Merge with {1}?", "Wins are added up into one card", "Merge", "Cards merged!", "%s Lv%u  solo %u / team %u",
    "Tap when the light is in the yellow!", "Perfect!", "Good!", "Miss", "%u rocks", "Perfect %u, best combo %u", "Combo x%u",
    "Tilt & collect berries", "Tilt the board to roll the ball", "Berry +1", "Gold +3", "Hole: -5 s", "As many as you can in 30 s!",
    "Start", "Set direction again", "Tilt the board to the RIGHT", "Now tilt it DOWN (toward you)", "Direction set!", "%u",
    "%u s", "%u berries", "The way you hold it now counts as level", "Hole! -5 s",
    "Update from home WiFi", "Looking for saved WiFi...", "Connecting to %s...", "Type this in the PC browser",
    "WiFi: %s (same router as the PC)", "Turns off by itself in %u min", "No saved WiFi",
    "Add one first in [SET UP WIFI]", "Could not join the WiFi", "Move closer and retry (2.4 GHz only)", "Retry",
    "Office WiFi may block PC-to-device access",
  },
  // ---------------- KO ----------------
  {
    "네트워크", "WiFi 미설정", "지금 시간 맞추기", "WiFi 설정하기", 
    "연결 중...", "시간 받는 중...", "시간 맞춤 완료!", "WiFi 연결 실패", "시간 서버 실패", "무선 사용 중",
    "최근: %02u/%02u %02u:%02u", "동기화 기록 없음", 
    "시간대", "위쪽 탭: 닫기",
    "야생 배틀", "통신 2인", "{1}{은} 무엇을 할까?",
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
    "보관함으로 보냈어요", "보관함이 가득 찼어요", "잡았다!",
    "보관함 %u/%u", "비어 있어요", "배틀에서 이기거나 잡으면 들어와요", "다음 육성 포켓몬은 여기서 나와요",
    "놓아주기", "닫기", "포획", "승리", "보관함에서 {1}{이} 나왔다!", "포켓볼 %u   물약 %u",
    "놓아줄까요?",
    "훈련 선택", "공격: 타이밍 펀치", "방어: 타이밍 맞춰 막기", "속도: 순서대로 터치",
    "공놀이: 기울여 열매 (기분)", "볼이 초록 칸에 오면 터치!", "1부터 순서대로 터치!",
    "방어 %u점", "방어 +%u", "속도 +%u", "좋아!", "놓쳤다",
    "소리/진동 설정", "배경음", "포켓몬 목소리", "시스템음", "소리 켜짐", "소리 꺼짐", "완료",
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
    "{1}{은} 이미 있어요!", "보관함 (사탕 %u)", "사탕 %u개로", "{1} 사탕 +{2}", "{1} 사탕 {2}개", 
    "경험치 올리기", "기분 기력 포만 +20", "능력 편차 올리기", "다음 알 샤이니 UP", "실수 만회 1회", "샤이니 UP 적용 중",
    "사용했어요!", "지금은 쓸 수 없어요",
    "비가 내린다! (물 강함, 불 약함)", "햇볕이 강하다! (불 강함, 물 약함)", "눈이 내린다! (얼음 강함)",
    "체육관", "오늘의 도전", "체육관 (배지 %u/8)", "도전!", "배지 %u개 필요",
    "관장 {1}{이} 승부를 걸어왔다!", "{1}{은} {2}{을} 내보냈다!", "트레이너 배틀에서는 안 돼요!", "{1} 획득! ({2}/8)",
    "웅", "이슬", "마티스", "민화", "독수", "초련", "강연", "비주기",
    "회색배지", "블루배지", "오렌지배지", "무지개배지", "핑크배지", "골드배지", "크림슨배지", "그린배지",
    "%s (배지%u)", "배지를 더 모으면 열려요",
    "{1}에서 3연전", "보상: 볼+2 물약+2 사탕+3", "오늘은 완료! (다시 해도 보상 없음)",
    "시계를 먼저 맞춰 주세요", "오늘의 도전! {1}에서 3연전", "오늘의 도전 성공! 볼+2 물약+2 사탕+3",
    "지금까지 %u번 성공", "상대",
    "월", "일", "친구 다마포케에서 시간을 받았어요",
    "x%u",
    "키움", "다음 파트너를 골라 주세요", "새 알 (무작위)", "{1} Lv.1부터 시작",
    "이미 키운 계열", "아래로 밀면 새 알로 시작",
    "     리본 %u", "끝까지 키운 포켓몬이 여기에 남아요", "끝까지 키운 포켓몬 (기념, 놓아줄 수 없음)",
    "실수 회복까지 %lu시간 %lu분", "실수 회복까지 %lu분", "모든 스탯 40 이상 12시간 = 실수 -1",
    "%u점", "맞힘 %u/%u  평균 %u.%02u초",
    "경험치 +%lu", "경험치 +%lu  사탕 +1", "사탕 +1", "쓰러뜨린 상대 %u마리", "필살기 %u번  총 피해 %u",
    "심심해...", "배고파", "씻고 싶어", "추워",
    "(탭: 원인)", "원인 기록 없음 (예전 실수)", "포만이 바닥났어요", "기분이 바닥났어요", "기력이 바닥났어요",
    "청결이 바닥났어요", "최근: %s %s",
    "재대전 %u승 (오늘 보상 있음)", "재대전 %u승 (오늘 보상 받음)", "재대전 보상! 사탕+2 볼+1 물약+1",
    "오늘 보상은 받았어요 (%u승)", "%s  %s  ~Lv%u",
    "챔피언 도전!", "배지 8개를 모으면 도전", "우승 %u회",
    "명예의 전당", "챔피언이 되면 여기에 남아요", "챔피언",
    "챔피언 리그! 6마리를 이겨라!", "챔피언 %u연승! 사탕+10 볼+3 물약+3", "보관함에 %u마리",
    "만능 사탕을 얻었다!", "사탕 가방", "사탕 가방 >", "만능 사탕 %u개", "%s 사탕 %u개", "지금", "아직 사탕이 없어요",
    "3개 -> 지금 계열 사탕 1개", "1번 교환", "모두 (%u)", "사용하러 가기", "1개 -> 지금 계열 사탕 5개", "사용", "닫기",
    "바꿨어요!", "알일 때는 쓸 수 없어요", "지금 키우는 계열이에요", "%u/%u",
    "1세대 (1~151)", "2세대 (152~251)",
    "명예의 전당 (%u)", "최근: %s", "%u번째 챔피언", "%u.%02u.%02u 우승", "능력 편차  공%u 방%u 속%u", "탭: 목록으로",
    "포켓볼 +1", "물약 +1", "그리기 평균 %u 최대 %ums  멈춤 최대 %ums", 
    "세이브 백업", "세이브 백업 (SD)", "지금 백업", "비어 있음", "최신", "SD카드가 없어요", "백업했어요!", "백업 실패",
    "SD카드에 백업이 있어요", "복원", "취소", "이 백업으로 복원할까요?", "지금 진행 상황은 사라져요", "백업을 누르면 복원",
    "자동: 하루 1번 + 진화, 챔피언, 작별 때", "복원 중... 다시 시작해요",
    "연승 %u  (최고 %u연승)", "연승이 끊겼어요 (%u연승)", "%u연승", "수동", "자동",
    "오늘의 이벤트: %s 포켓몬이 자주 나와요", "주말 이벤트: 색이 다른 포켓몬 2배", "보름달 밤: 전설 포켓몬 3배!",
    "친밀도 %u/%u 필요",
    "탐험", "탐험 중", "몇 시간 탐험 보낼까요?", "%u시간", "탐험 중: %s  %u:%02u 남음", "%s 귀환! 눌러서 받기",
    "%s 탐험 결과", "%s 사탕 +%u", "볼 +%u  물약 +%u", "만능 사탕 +1!", "새 친구를 데려왔어요: %s", "보관함이 가득 찼어요",
    "%s가 탐험에서 돌아왔어요 (보관함)", "%s가 탐험을 떠났어요", "고향 %s(으)로 떠나요", "눌러서 닫기",
    "도감 %u종 달성! %s", "볼 +5", "물약 +5", "만능 사탕 +1", "만능 사탕 +3", "만능 사탕 +5, 샤이니 부적", "만능 사탕 +5",
    "만능 사탕 +10, 샤이니 부적", "잡음 %u/251  다음 보상 %u종", "잡음 %u/251  모든 보상 완료",
    "배경음 고르기 >", "배경음 고르기", "켠 곡만 무작위로 나와요", "%u번 곡", "한 곡은 켜 둬야 해요",
    "SD카드에 bgm%u.wav ~ bgm8.wav를 넣으면 늘어나요", "켬", "끔",
    "{1}{이} 따라오고 싶어 해요!", "데려갈까요?", "데려가기", "보내주기", "{1}{은} 집으로 돌아갔어요", "{1}{이} 보관함에 들어왔어요!", "{1}의 반격!",
    "배구: 5점 먼저 내기", "5점 먼저 내면 승리!", "포켓몬이 공을 따라 알아서 움직여요",
    "톡 = 점프, 점프해서 공을 치면 스파이크!", "득점!", "실점...", "승리!", "아쉬워요...",
    "%u연승 중 (최고 %u연승)", "속도 +%u", "스파이크!", "눌러서 닫기", "점수를 2초 누르면 그만",
    "재부팅 기록 %u회 >", "재부팅 기록", "마지막: %s", "원인: %s", "위치: %s %u.%u", "횟수: %u회",
    "프로그램 오류로 멈춤", "응답이 없어서 스스로 다시 시작", "전원이 부족해서 꺼짐",
    "전체 기록: SD카드 tpsave/crash.txt", "기록 지우기", "닫기",
    "최고 %u / 역대 %u", "역대 신기록!",
    "아직 어려서 키운 기록에 안 남아요", "지금 %u연승 / 최고 %u / 역대 %u", "%s 강스파이크!",
    "퍼펙트!", "좋아!", "너무 빨라요", "너무 늦어요", "%u 콤보", "퍼펙트 %u  좋아 %u",
    "연타해서 기를 모으면 필살기!", "순서가 틀렸어요",
    "조각 %u/%u", "남은 사탕을 만능 조각으로", "사탕 %u개를 조각으로!", "바꿀 사탕이 없어요",
    "%s 공격구슬", "%s 방어구슬", "공격 +%u%%", "방어 +%u%%", "장착", "빼기",
    "장착 중", "%s 속성 포켓몬만 쓸 수 있어요", "아직 구슬이 없어요", "야생 승리·체육관·탐험에서 가끔 나와요",
    "%s +%u%% 획득!", "%s +%u%% 강화! 예전 구슬은 사탕 1개", "%s +%u%%: 더 약해서 사탕 1개",
    "진화로 속성이 바뀌어 구슬이 사탕 3개로", "진화로 속성이 바뀌어 구슬이 만능 사탕 1개로", "%u/%u",
    "그림: 도트", "그림: 원작",
    "화면 밝기", 
    "체력 %u 회복!",
    "{1}{은} 잘 잤어요!",
    "구슬 합성", "합성하기 %u/3", "취소", "합성 성공!", "대성공!", "합성 실패...", "위로의 조각 +2", "알일 때는 합성할 수 없어요", "구슬이 3개 이상 필요해요", "장착 중", "탭해서 닫기",
    "자동", "%u마리", "자동 전투 중  남은 %u마리", "자동 전투를 멈췄어요", "화면을 누르면 멈춰요",
    "도우미 고르기 %u/%u", "상대: %s 속성", "상대 %u마리 (나올 때마다 확인)", "유리", "불리", "혼자", "시작 %u/%u",
    "누구를 내보낼까요?", "다음은 {1}! 교체할까요?", "그대로", "누구로 바꿀까요? (한 턴 써요)", "가라, {1}!", "도우미 Lv+1 (%u마리)",
    "단독", "팀", "오늘 %u/%u",
    "단독+팀", "단독 %u승 / 팀 %u승", "금관으로 승격!",
    "완벽 육성", "완벽 육성 (메달 8/8)",
    "충전 한도", "100%", "약 90% (수명 연장)", 
    "진동 강", "진동 꺼짐", "진동 중", "진동 약",
    "설정", "시간/날짜", "소리/진동", "화면/배터리", "언어 %s >", 
    "좋아하는 먹이", "주면 배가 더 차고 기분과 유대가 올라요", "여러 먹이를 줘 보면 알게 돼요",
    "함께한 지 %lu일째", "(%04u.%02u.%02u부터)",
    "새 기술 {1}{을} 배웠다!",
    "싸운다", "{1}{은} {2}{을} 배우고 싶다!", "기술은 4개까지. 어떤 기술을 잊을까?", "새 기술: {1}", "배우지 않는다", "쓸 수 있는 기술이 없다!",
    "공격", "방어", "스피드", "{1}의 {2}{이} 올랐다!", "{1}의 {2}{이} 크게 올랐다!", "{1}의 {2}{이} 내려갔다!", "{1}의 {2}{이} 크게 내려갔다!", "{1}의 {2}{은} 더는 오르지 않는다", "{1}의 {2}{은} 더는 내려가지 않는다",
    "{1}{은} 독에 걸렸다!", "{1}{은} 화상을 입었다!", "{1}{은} 마비됐다!", "{1}{은} 잠들었다!", "{1}{은} 얼어붙었다!", "{1}{은} 혼란스러워한다!",
    "독", "화상", "마비", "잠듦", "얼음", "혼란",
    "하지만 통하지 않았다!", "{1}{은} 독 때문에 아프다!", "{1}{은} 화상 때문에 아프다!", "{1}{은} 몸이 저려 움직이지 못한다!", "{1}{은} 쿨쿨 자고 있다", "{1}{은} 얼어서 움직이지 못한다!", "{1}{은} 움찔해서 움직이지 못한다!", "{1}{은} 반동 때문에 움직일 수 없다!",
    "{1}{은} 잠에서 깼다!", "{1}의 얼음이 녹았다!", "{1}{은} 정신을 차렸다!", "{1}의 독이 빠졌다!", "{1}의 화상이 나았다!", "{1}의 마비가 풀렸다!", "{1}{은} 혼란스러워 스스로를 때렸다!", "{1}{은} 반동을 받았다!", "{1}{은} 체력을 빨아들였다!", "쓸 수 있는 기술이 없다!",
    "포켓몬센터", "회복하기", "회복 중... %u초", "PP가 모두 찼어요!", "회복 중이에요", "PP가 가득해요", 
    "위력 %u  명중 %u  PP %u", "변화 기술", 
    "{1}{이} 고마워하며 떠났어요!", "포켓볼 +1", "물약 +1", "조각 +1", "경험치 +{2}",
    "이별 조건", "최종 진화형", "함께한 시간 3일", "진화 필요", "%u일 %u시간", "3일까지 %u시간 남음", "모두 충족! 기본 화면에 이별 버튼", "진화하면 이별할 수 있어요",
    "초원", "바닷가", "숲", "화산", "산", "설원", "발전소", "도장",
    "늪", "사막", "유적", "꽃밭", "묘지", "용의 계곡", "도시", "광산",
    "%s 무지개구슬", "공격/방어 +%u%%", "무지개구슬 탄생!",
    "진화로 속성이 바뀌어 무지개구슬이 사탕 10개로", "진화로 속성이 바뀌어 무지개구슬이 사탕 10개 + 만능 사탕 1개로",
    "추억", "%s의 추억", "추억 기록이 생기기 전에 키웠어요", "(v1.17-ko12.4부터 기록돼요)", "함께한 날 %u일",
    "알에서 태어났어요", "보관함에서 함께하게 됐어요", "교환으로 왔어요", "기록은 업데이트한 날부터",
    "진화: %s  %s", "첫 승리 %s   야생 승리 %u번", "아직 이긴 적이 없어요", "Lv.%u에서 작별  %s", "별명: %s", "기록",
    "밥 %u   간식 %u   청소 %u", "쓰다듬기 %u   놀이 %u   훈련 %u", "통신 대전 승리 %u   오늘의 대전 %u",
    "체육관 배지 +%u   챔피언 %u번", "친밀도 %u   돌봄 실수 %u   메달 %u개", "공놀이 %u   샌드백 %u   방어 %u", "속도 %u   배구 %u연승",
    "탭: 다음 쪽",
    "쿠션", "화분", "비치볼", "등불", "트로피", "곰인형",
    "처음부터", "7일 연속 돌보기", "공놀이 20점", "1만 걸음/50승", "챔피언 1번", "도감 100종",
    "방 꾸미기", "야외", "방", "왼쪽", "앞", "오른쪽", "물건을 고른 뒤 자리를 누르세요",
    "놓을 자리를 누르세요", "방 꾸미기", "산책", "오늘의 산책", "움직임 센서를 찾지 못했어요", "(이 보드에는 QMI8658이 없어요)",
    "%s 걸음", "목표 %u걸음까지 %u", "오늘 목표를 모두 채웠어요!", "최근 7일 (오른쪽이 오늘)",
    "2,000  기분 +20, 친밀도 +2", "5,000  사탕 1개", "10,000  만능 사탕 조각 2",
    "2,000 걸음! 기분 +20", "5,000 걸음! 사탕 +1", "10,000 걸음! 조각 +2", "전체 %lu 걸음",
    "흔들면 깨우거나 신나해요", "흔들어서 깼어요!", "와아! 흔들어 줘서 신났어요",
    "배경을 골라 주세요", "설정 > 방 꾸미기에서 언제든 바꿀 수 있어요",
    "떼쓰고 있어요!", "필요한 건 없는데 관심을 받고 싶어해요", "혼내기", "달래기",
    "떼쓸 때 혼내면 의젓해져요", "얌전해졌어요. 훈육 +", "원하는 걸 얻었어요... 훈육 -", "떼쓰는 중!",
    "아침밥 완료! (하루 일과)", "낮 놀이 완료! (하루 일과)", "밤 재우기 완료! (하루 일과)", "규칙적인 하루! 연속 %u일",
    "아침밥  6~11시", "낮 놀이  12~19시", "밤 재우기 (불 끄기)  20~24시",
    "아직 몰라요", "먹보", "장난꾸러기", "응석쟁이", "노력파", "깔끔이", "의젓함",
    "더 돌보면 드러나요", "밥을 먹으면 조금 더 배불러요", "놀아 주면 더 기뻐해요", "쓰다듬으면 더 기뻐해요",
    "훈련 경험치가 더 많아요", "청소해 주면 기뻐해요", "떼를 거의 안 써요",
    "생활", "성격: %s", "훈육", "지금 떼쓰는 중: 포켓몬을 눌러 보세요", "오늘의 일과", "규칙적인 날 %u일 연속 (최고 %u일)",
    "감기를 내버려 둠", "감기에 걸렸어요...", "에취! 감기에 걸렸어요. 포켓몬을 눌러 약을 주세요", "비를 맞으며 걸어서 감기에 걸렸어요!", "감기를 너무 오래 내버려 뒀어요 (실수 +1)", "감기에 걸렸어요", "약을 %u번 더 먹어야 나아요", "약 먹이기", "나중에", "더럽거나 배고프면 감기에 걸리기 쉬워요", "다음 약은 %u분 뒤에 먹을 수 있어요", "꿀꺽! 3분 뒤에 한 번 더 먹어요", "다 나았어요!", "{1}{이} 놀러 왔어요!", "{1}{와} 함께 놀았어요! 기분 +10", "{1}{은} 보관함으로 돌아갔어요", "내 생일", "내 생일", "설정 안 함", "%u월 %u일", "그날 포켓몬이 폭죽과 함께 축하해 줘요", "지우기", "저장", "생일 축하해요!", "{1}{이} 축하 노래를 불러요!", "선물: 기분 +30, 만능 사탕 1개", "눌러서 닫기", "생일을 저장했어요!", "%u월", "%u일", "약 먹을 시간이에요! 포켓몬을 눌러 주세요",
    "축하해요! 모든 포켓몬을 키웠어요", "축하해요! 모두 반짝이는 모습으로 키웠어요", "%u계열 모두 함께했어요", "함께한 친구들", "%u번째 친구", "함께한 여정", "여정을 시작한 날  %04u.%02u.%02u", "함께한 날  %lu일", "명예의 전당  %u마리", "도감  %u / 251", "색이 다른 친구  %u마리", "함께 걸은 걸음  %s", "야생 배틀 승리  %u번", "챔피언  %u번", "통신 대전 승리 %u번  교환 %u번", "체육관 배지  %u / 8", "그리고, 정말 고마워요.", "처음 만난 날부터 오늘까지 정말 고마워.", "이제 반짝이는 여정을 함께 떠나자!", "반짝이는 모두와 함께한 시간, 잊지 않을게.", "너는 진짜 포켓몬 마스터야.", "칭호: 포켓몬 마스터 / 이제 색이 다른 포켓몬 2배", "칭호: 빛나는 포켓몬 마스터", "눌러서 계속", "누르면 다음으로", "엔딩 다시 보기", "원하는 알 고르기 (%u)", "원하는 알 고르기", "아직 반짝이는 모습으로 키우지 않은 계열", "{1}의 알이 됐어요!", "고를 계열이 없어요", "여정 %u/%u계열", "반짝이는 여정 %u/%u계열",
    "SD 파일 하나로 묶기", "SD 파일 묶기", "파일 %u개 · %uMB", "암호화된 파일 하나(mons.pak)로 만들어요", "다른 SD에는 mons.pak 하나만 복사하면 돼요", "묶기 시작", "사용 중: mons.pak (파일 %u개)", "mons.pak 암호가 맞지 않아요", "mons.pak가 손상됐어요", "묶는 중... 전원을 끄지 마세요", "완료! mons.pak을 만들었어요", "SD 남은 공간이 부족해요", "SD 오류로 실패했어요", "mons 폴더에 파일이 없어요", "%u / %u개", "아직 mons.pak 없음 (낱개 파일 사용 중)",
    "막아서 피해가 절반이 됐다!", "효과가 굉장했지만 막아서 절반!", "효과가 별로인 데다 막혀 버렸다...", "다음 턴엔 이 기술을 쉬어야 해요", "쉬는 중",
    "SD 파일 점검", "SD 파일 점검", "포켓몬 그림", "배틀 그림", "기술 이펙트", "울음소리", "작은 그림", "스토리 인물", "음악 (없어도 됨)", "필요한 파일이 모두 있어요!", "없는 파일: %s 외 %u개", "SD카드가 없어요", "mons.pak 안에서 확인 (파일 %u개)",
    "사탕 %u개",
    "같은 포켓몬 카드 합치기", "합칠 카드를 고르세요", "{1} 카드와 합칠까요?", "우승 횟수를 더해서 카드 1장으로 만들어요", "합치기", "카드를 합쳤어요!", "%s Lv%u  단독 %u · 팀 %u",
    "빛이 노란 칸에 올 때 탭!", "완벽!", "좋아!", "빗나감", "바위 %u개", "완벽 %u번, 최대 콤보 %u", "콤보 x%u",
    "기울여 열매 모으기", "기기를 기울이면 몬스터볼이 굴러가요", "열매 +1", "금색 +3", "구멍: 시간 -5초", "30초 동안 최대한 많이!",
    "시작", "방향 다시 맞추기", "기기를 오른쪽으로 기울여 주세요", "이번엔 아래쪽(내 쪽)으로 기울여 주세요", "방향을 맞췄어요!", "%u개",
    "%u초", "열매 %u개", "편하게 든 지금 기울기가 기준이에요", "구멍! -5초",
    "집 와이파이로 업데이트", "저장된 와이파이 찾는 중...", "%s에 연결하는 중...", "PC 브라우저 주소창에 입력하세요",
    "와이파이: %s (PC와 같은 공유기)", "%u분 뒤 저절로 꺼져요", "저장된 와이파이가 없어요",
    "[WiFi 설정하기]에서 먼저 등록해 주세요", "와이파이에 연결하지 못했어요", "공유기 가까이에서 다시 (2.4GHz만 돼요)", "다시 하기",
    "회사 와이파이는 기기끼리 막혀 있을 수 있어요",
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

// ko11.31: las otras dos variantes (1, 2) de cada tipo y fase; la 0 es la de MOVES_*
static const char *const MOVES2_KO[3][PT_COUNT][2] = {
  {
    { "전광석화", "할퀴기" },
    { "회오리불꽃", "불꽃펀치" },
    { "거품", "아쿠아제트" },
    { "흡수", "씨기관총" },
    { "스파크", "번개펀치" },
    { "얼음뭉치", "냉동펀치" },
    { "발차기", "바위깨기" },
    { "용해액", "독찌르기" },
    { "뼈다귀치기", "머드숏" },
    { "사이코웨이브", "사념의박치기" },
    { "벌레먹음", "흡혈" },
    { "암석봉인", "구르기" },
    { "놀래키기", "섀도펀치" },
    { "용의숨결", "드래곤테일" },
    { "도둑질", "깜짝베기" },
    { "불릿펀치", "미러숏" },
  },
  {
    { "베어가르기", "돌진" },
    { "열풍", "화염자동차" },
    { "파도타기", "아쿠아테일" },
    { "기가드레인", "꽃잎댄스" },
    { "와일드볼트", "방전" },
    { "얼음엄니", "오로라빔" },
    { "이단차기", "파동탄" },
    { "크로스포이즌", "오물웨이브" },
    { "뼈다귀부메랑", "땅고르기" },
    { "사이코커터", "미래예지" },
    { "시저크로스", "시그널빔" },
    { "파워젬", "락블레스트" },
    { "섀도크루", "괴상한바람" },
    { "드래곤크루", "용의파동" },
    { "악의파동", "기습" },
    { "아이언헤드", "러스터캐논" },
  },
  {
    { "기가임팩트", "이판사판태클" },
    { "블래스트번", "오버히트" },
    { "하이드로캐논", "폭포오르기" },
    { "하드플랜트", "리프스톰" },
    { "볼트태클", "전자포" },
    { "고드름떨구기", "프리즈드라이" },
    { "인파이트", "기합구슬" },
    { "더스트슈트", "베놈쇼크" },
    { "땅가르기", "대지의힘" },
    { "사이코부스트", "사이코쇼크" },
    { "벌레의야단법석", "덤벼들기" },
    { "스톤에지", "양날박치기" },
    { "섀도다이브", "병상첨병" },
    { "드래곤다이브", "용성군" },
    { "속임수", "나이트버스트" },
    { "코멧펀치", "헤비봄버" },
  },
};
// ko11.31: las otras dos variantes (1, 2) de cada tipo y fase; la 0 es la de MOVES_*
static const char *const MOVES2_EN[3][PT_COUNT][2] = {
  {
    { "QUICK ATTACK", "SCRATCH" },
    { "FIRE SPIN", "FIRE PUNCH" },
    { "BUBBLE", "AQUA JET" },
    { "ABSORB", "BULLET SEED" },
    { "SPARK", "THUNDERPUNCH" },
    { "ICE SHARD", "ICE PUNCH" },
    { "LOW KICK", "ROCK SMASH" },
    { "ACID", "POISON JAB" },
    { "BONE CLUB", "MUD SHOT" },
    { "PSYWAVE", "ZEN HEADBUTT" },
    { "BUG BITE", "LEECH LIFE" },
    { "ROCK TOMB", "ROLLOUT" },
    { "ASTONISH", "SHADOW PUNCH" },
    { "DRAGONBREATH", "DRAGON TAIL" },
    { "THIEF", "SUCKER PUNCH" },
    { "BULLET PUNCH", "MIRROR SHOT" },
  },
  {
    { "SLASH", "TAKE DOWN" },
    { "HEAT WAVE", "FLAME WHEEL" },
    { "SURF", "AQUA TAIL" },
    { "GIGA DRAIN", "PETAL DANCE" },
    { "WILD CHARGE", "DISCHARGE" },
    { "ICE FANG", "AURORA BEAM" },
    { "DOUBLE KICK", "AURA SPHERE" },
    { "CROSS POISON", "SLUDGE WAVE" },
    { "BONEMERANG", "BULLDOZE" },
    { "PSYCHO CUT", "FUTURE SIGHT" },
    { "X-SCISSOR", "SIGNAL BEAM" },
    { "POWER GEM", "ROCK BLAST" },
    { "SHADOW CLAW", "OMINOUS WIND" },
    { "DRAGON CLAW", "DRAGON PULSE" },
    { "DARK PULSE", "PURSUIT" },
    { "IRON HEAD", "FLASH CANNON" },
  },
  {
    { "GIGA IMPACT", "DOUBLE-EDGE" },
    { "BLAST BURN", "OVERHEAT" },
    { "HYDRO CANNON", "WATERFALL" },
    { "FRENZY PLANT", "LEAF STORM" },
    { "VOLT TACKLE", "ZAP CANNON" },
    { "ICICLE CRASH", "FREEZE-DRY" },
    { "CLOSE COMBAT", "FOCUS BLAST" },
    { "GUNK SHOT", "VENOSHOCK" },
    { "FISSURE", "EARTH POWER" },
    { "PSYCHO BOOST", "PSYSHOCK" },
    { "BUG BUZZ", "LUNGE" },
    { "STONE EDGE", "HEAD SMASH" },
    { "SHADOW FORCE", "HEX" },
    { "DRAGON RUSH", "DRACO METEOR" },
    { "FOUL PLAY", "NIGHT DAZE" },
    { "METEOR MASH", "HEAVY SLAM" },
  },
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

const char *moveName(uint8_t move, uint8_t type, uint8_t tier, uint8_t var) {
  if (tier > 2) tier = 2;
  if (move == BA_TYPE && type < PT_COUNT && var >= 1 && var <= 2)  // ko11.31
    return isKo() ? MOVES2_KO[tier][type][var - 1] : MOVES2_EN[tier][type][var - 1];
  if (move == BA_TYPE && type < PT_COUNT) return isKo() ? MOVES_KO[tier][type] : MOVES_EN[tier][type];
  return XT(X_M_TACKLE);
}

// ko11.31: nombre por id (battle.h): de tipo, placaje, de estado y forcejeo
const char *moveNameId(uint8_t id) {
  uint8_t t, s, v;
  if (moveDecode(id, &t, &s, &v)) return moveName(BA_TYPE, t, s, v);
  if (id == MOVE_STRUGGLE) return isKo() ? "발버둥" : "STRUGGLE";
  if (id >= MOVE_TACKLE && id < MOVE_N) return isKo() ? MOVE_X_KO[id - MOVE_TACKLE] : MOVE_X_EN[id - MOVE_TACKLE];
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
  return ((cp - 0xAC00) % 28) ? (((cp - 0xAC00) % 28) == 8 ? 2 : 1) : 0;  // ko11.30: 2 = batchim ㄹ (para {로})
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
    { "{아}", "아", "야" },  // ko11.21: vocativo (피카츄야 / 꼬부기야)
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
        append(out, n, len, b >= 1 ? j[1] : j[2]);  // latino o sin hangul: forma vocal
        p += tl;
        done = true;
        break;
      }
    }
    if (done) continue;
    if (strncmp(p, "{로}", strlen("{로}")) == 0) {  // ko11.30: 으로 / 로 (sin batchim o con ㄹ: 로)
      int b = lastBatchim(out, len);
      append(out, n, len, b == 1 ? "으로" : "로");
      p += strlen("{로}");
      continue;
    }
    char one[2] = { *p++, 0 };
    append(out, n, len, one);
  }
}

void txFmt(char *out, size_t n, XId id, const char *a1, const char *a2) {
  txFmtRaw(out, n, XT(id), a1, a2);
}
