// ko11.9: fisica e IA del voleibol (volley.h)
#include "framework.h"
#include "../volley.h"

// el lado 0 jugado por un "jugador" sencillo: va a donde caera la pelota y a veces salta
static void mirrorAi(VolleyGame &g, VbRng &r) {
  static uint32_t next = 0; static float tgt = 140; if (g.t < next) next = 0;
  if (g.t >= next) { next = g.t + 200;
  float head = g.bodyY(0) - g.p[0].hitR;
  float tx = VolleyGame::homeX(0);
  if (g.state == VB_PLAY && (g.b.x < VB_NET_X || g.b.vx < 0)) {
    float lx = g.predictX(head);
    if (lx < VB_NET_X) tx = lx - 12;
  }
  tgt = tx + (float)((int)r.below(41) - 20); }
  g.moveTo(0, tgt);
  if (g.state == VB_PLAY && !g.airborne(0) && g.b.vy > 0 && fabsf(g.b.x - g.p[0].x) < 40 &&
      g.bodyY(0) - g.b.y > 80 && g.bodyY(0) - g.b.y < 180 && r.below(100) < 20) g.jump(0);
  if (g.airborne(0) && r.below(100) < 30) g.spike(0);
}

TEST(Volley, games_end_and_ball_stays_on_court) {
  for (uint32_t seed = 1; seed <= 40; seed++) {
    VolleyGame g;
    g.begin(seed, seed % 6);
    VbRng r(seed * 7);
    uint32_t ms = 0;
    bool inside = true, capped = true;
    uint16_t rallies = 0;
    uint8_t lastTotal = 0;
    while (g.state != VB_OVER && ms < 20 * 60 * 1000) {
      mirrorAi(g, r);
      g.step(20);
      ms += 20;
      if (g.b.x < VB_LEFT - 1 || g.b.x > VB_RIGHT + 1 || g.b.y > VB_GROUND + 1 || g.b.y < VB_CEIL - 1) inside = false;
      if (sqrtf(g.b.vx * g.b.vx + g.b.vy * g.b.vy) > VB_MAXV + 5 && g.state == VB_PLAY) {
        // la gravedad puede sumar un poco despues de un golpe, pero nunca mucho
        if (sqrtf(g.b.vx * g.b.vx + g.b.vy * g.b.vy) > VB_MAXV * 1.6f) capped = false;
      }
      for (int s = 0; s < 2; s++)
        if (g.p[s].x < VolleyGame::minX(s) - 0.5f || g.p[s].x > VolleyGame::maxX(s) + 0.5f) inside = false;
      if (g.score[0] + g.score[1] != lastTotal) { rallies++; lastTotal = g.score[0] + g.score[1]; }
    }
    CHECK_EQ(g.state, (uint8_t)VB_OVER);
    CHECK(g.score[0] == VB_WIN || g.score[1] == VB_WIN);
    CHECK(g.score[0] <= VB_WIN && g.score[1] <= VB_WIN);
    CHECK_EQ(g.winner, (uint8_t)(g.score[1] > g.score[0]));
    CHECK(inside);
    CHECK(capped);
    CHECK(g.hits[0] + g.hits[1] > rallies);  // hubo peloteo, no solo saques al suelo
  }
}

TEST(Volley, harder_ai_wins_more) {
  int wins[2] = {0, 0};
  for (int lvl = 0; lvl < 2; lvl++)
    for (uint32_t seed = 1; seed <= 60; seed++) {
      VolleyGame g;
      g.begin(seed + 1000, lvl ? 5 : 0);
      g.p[1].speed = lvl ? 250 : 170;
      g.p[1].dig = lvl ? 85 : 40;
      VbRng r(seed);
      for (uint32_t ms = 0; g.state != VB_OVER && ms < 20 * 60 * 1000; ms += 20) { mirrorAi(g, r); g.step(20); }
      if (g.winner == 1) wins[lvl]++;
    }
  CHECK(wins[1] > wins[0]);
}

TEST(Volley, jump_spike_and_serve) {
  VolleyGame g;
  g.begin(3, 0);
  CHECK_EQ(g.state, (uint8_t)VB_SERVE);
  g.step(VB_SERVE_MS + 10, false);
  CHECK_EQ(g.state, (uint8_t)VB_PLAY);
  g.jump(0);
  g.step(250, false);         // cerca de lo mas alto del salto
  CHECK(g.airborne(0));
  g.jump(0);                  // en el aire no salta otra vez
  float vy = g.p[0].vy;
  CHECK(vy < VB_JUMP_V);
  // remate: la pelota junto a la cabeza en el aire sale fuerte hacia la derecha
  g.spike(0);
  g.b.x = g.p[0].x + 10; g.b.y = g.bodyY(0) - g.p[0].hitR; g.b.vx = 0; g.b.vy = 50;
  g.substep(0.01f, 10);
  CHECK(g.b.spiked);
  CHECK(g.b.vx > 400);
  CHECK(g.b.vy > 0);          // hacia abajo
  CHECK_EQ(g.spikes[0], (uint16_t)1);
  // y cae dentro del otro campo, pasando por encima de la red
  {
    VolleyGame c = g;
    bool over = true;
    for (int i = 0; i < 300 && c.state == VB_PLAY; i++) {
      if (fabsf(c.b.x - VB_NET_X) < VB_NET_HW + VB_BALL_R && c.b.y + VB_BALL_R > VB_NET_TOP) over = false;
      c.p[1].coolUntil = 0xFFFFFFFFu;  // el rival no la toca
      c.substep(0.01f, 10);
    }
    CHECK(over);
    CHECK_EQ(c.state, (uint8_t)VB_POINT);
    CHECK_EQ(c.score[0], (uint8_t)1);
  }
  // un remate mal recibido (dig 0 %) cae en el campo de quien lo recibe
  {
    VolleyGame c;
    c.begin(9, 0);
    c.step(VB_SERVE_MS + 10, false);
    c.p[1].dig = 0;
    c.b.x = c.p[1].x - 20; c.b.y = c.bodyY(1) - c.p[1].hitR - 5; c.b.vx = -500; c.b.vy = 200;
    c.b.spiked = true; c.b.spikeSide = 0;
    for (int i = 0; i < 300 && c.state == VB_PLAY; i++) c.substep(0.01f, 10);
    CHECK_EQ(c.score[0], (uint8_t)1);
  }
  // suelo del lado derecho: punto para el jugador
  VolleyGame h;
  h.begin(5, 0);
  h.step(VB_SERVE_MS + 10, false);
  h.b.x = 330; h.b.y = VB_GROUND - VB_BALL_R - 1; h.b.vx = 0; h.b.vy = 300;
  h.p[1].x = 400;  // lejos
  h.substep(0.01f, 10);
  CHECK_EQ(h.state, (uint8_t)VB_POINT);
  CHECK_EQ(h.score[0], (uint8_t)1);
  h.step(VB_POINT_MS + 10, false);
  CHECK_EQ(h.state, (uint8_t)VB_SERVE);
  CHECK_EQ(h.server, (uint8_t)0);  // saca quien gano el punto
}
