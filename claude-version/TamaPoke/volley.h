#pragma once
// ko11.9: minijuego de voleibol (al estilo de "Pikachu Volleyball", 1997).
// Solo la fisica y la IA, sin pantalla ni Arduino: se prueba en el PC
// (test/test_volley.cpp). La pantalla y los toques estan en volley.ino.
//
// Coordenadas de pantalla (466x466, y hacia abajo). Lado 0 = izquierda (jugador),
// lado 1 = derecha (IA). p.y es la ALTURA del salto (0 = en el suelo).
#include <stdint.h>
#include <math.h>

#define VB_GROUND 388     // suelo de la pista
#define VB_NET_X 233      // red en el centro
#define VB_NET_TOP 316    // arriba de la red
#define VB_NET_HW 4       // media anchura de la red
#define VB_LEFT 44        // paredes (la pantalla es redonda: un poco hacia dentro)
#define VB_RIGHT 422
#define VB_CEIL 106       // techo (debajo del marcador)
#define VB_BALL_R 13
#define VB_BODY_UP 30     // centro del cuerpo sobre el suelo
#define VB_WIN 5          // puntos para ganar
#define VB_SERVE_MS 900
#define VB_POINT_MS 1100
#define VB_GRAV 560.0f    // px/s^2 (pelota "flotante", como el original)
#define VB_PGRAV 1250.0f  // gravedad de los jugadores
#define VB_JUMP_V 470.0f
#define VB_MAXV 720.0f
#define VB_SPIKE_MAXV 1000.0f  // remate: tope de velocidad

enum : uint8_t { VB_SERVE = 0, VB_PLAY, VB_POINT, VB_OVER };

struct VbPlayer {
  float x = 0, y = 0, vy = 0;  // x en pantalla, y = altura del salto
  float speed = 200;           // px/s andando (sube con la VEL)
  float spikePow = 520;        // velocidad del remate (sube con el ATQ)
  float hitR = 32;             // radio del cuerpo (sube un poco con la DEF)
  uint8_t dig = 55;            // % de recibir bien un remate (sube con la DEF)
  float targetX = 0;
  bool moving = false;
  uint32_t spikeUntil = 0;     // ventana del remate pedido en el aire
  uint32_t coolUntil = 0;      // no golpear dos veces seguidas
  uint32_t spikeT = 0;         // ultimo remate (para la animacion)
  bool jumped = false;         // la IA ya decidio en este salto
};

struct VbBall {
  float x = 0, y = 0, vx = 0, vy = 0;
  bool spiked = false;
  uint8_t spikeSide = 0;
};

struct VbRng {
  uint32_t s;
  explicit VbRng(uint32_t seed = 1) : s(seed ? seed : 0x2545F491u) {}
  uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }
};

struct VolleyGame {
  VbPlayer p[2];
  VbBall b;
  uint8_t score[2] = {0, 0};
  uint8_t state = VB_SERVE, server = 0, lastScorer = 0, winner = 0;
  uint32_t t = 0, stateT = 0;  // ms de juego / desde el cambio de estado
  uint8_t aiLevel = 0;         // 0..5
  float aiErr = 0;             // error de la IA en este punto
  uint32_t aiNext = 0;         // la IA solo "mira" cada cierto tiempo (reflejos)
  float aiTarget = 326;
  bool autoMove0 = false;      // ko11.9.1: el lado 0 corre solo hacia la pelota (el jugador solo salta/remata)
  uint32_t autoNext = 0;
  float autoTarget = 140;
  uint16_t hits[2] = {0, 0};   // toques de cada lado (estadistica)
  uint16_t spikes[2] = {0, 0};
  VbRng rng;

  static float minX(int s) { return s ? VB_NET_X + VB_NET_HW + 22 : VB_LEFT + 20; }
  static float maxX(int s) { return s ? VB_RIGHT - 20 : VB_NET_X - VB_NET_HW - 22; }
  static float homeX(int s) { return s ? 326 : 140; }
  float bodyY(int s) const { return VB_GROUND - VB_BODY_UP - p[s].y; }

  void begin(uint32_t seed, uint8_t level) {
    rng = VbRng(seed);
    aiLevel = level > 5 ? 5 : level;
    score[0] = score[1] = 0;
    hits[0] = hits[1] = spikes[0] = spikes[1] = 0;
    t = 0;
    server = 0;
    resetRally();
  }

  void resetRally() {
    for (int s = 0; s < 2; s++) {
      p[s].x = p[s].targetX = homeX(s);
      p[s].y = p[s].vy = 0;
      p[s].moving = false;
      p[s].spikeUntil = p[s].coolUntil = 0;
      p[s].jumped = false;
    }
    b.x = homeX(server) + (server ? -6 : 6);  // un poco hacia la red
    b.y = VB_GROUND - 190;
    b.vx = b.vy = 0;
    b.spiked = false;
    state = VB_SERVE;
    stateT = 0;
    aiErr = (float)((int)rng.below(121) - 60) * (1.0f - aiLevel * 0.16f);
    aiNext = 0;
    aiTarget = homeX(1);
    autoNext = 0;
    autoTarget = homeX(0);
  }

  // ---- entradas ----
  void moveTo(int s, float x) {
    p[s].targetX = x < minX(s) ? minX(s) : x > maxX(s) ? maxX(s) : x;
    p[s].moving = true;
  }
  void stop(int s) { p[s].moving = false; }
  bool airborne(int s) const { return p[s].y > 0.5f || p[s].vy > 0; }
  void jump(int s) {
    if (state == VB_OVER || airborne(s)) return;
    p[s].vy = VB_JUMP_V;
    p[s].jumped = false;
  }
  void spike(int s) {  // en el aire: el siguiente toque es un remate
    if (airborne(s)) p[s].spikeUntil = t + (s == 0 && autoMove0 ? 1000 : 380);  // jugador: todo el salto
  }
  // tocar arriba = saltar, o rematar si ya esta en el aire
  void action(int s) { if (airborne(s)) spike(s); else jump(s); }

  // ---- IA (lado 1) ----
  // donde cruzara la pelota la altura de la cabeza (sin contar a los jugadores)
  float predictX(float headY) const {
    float x = b.x, y = b.y, vx = b.vx, vy = b.vy;
    for (int i = 0; i < 300; i++) {
      float dt = 0.01f;
      vy += VB_GRAV * dt;
      x += vx * dt;
      y += vy * dt;
      if (x - VB_BALL_R < VB_LEFT) { x = VB_LEFT + VB_BALL_R; vx = fabsf(vx); }
      if (x + VB_BALL_R > VB_RIGHT) { x = VB_RIGHT - VB_BALL_R; vx = -fabsf(vx); }
      if (y - VB_BALL_R < VB_CEIL) { y = VB_CEIL + VB_BALL_R; vy = fabsf(vy) * 0.6f; }
      if (vy > 0 && y >= headY) return x;
    }
    return x;
  }

  void thinkAi() {
    const int s = 1;
    VbPlayer &me = p[s];
    float head = bodyY(s) - me.hitR;
    bool mine = b.x > VB_NET_X || (state == VB_PLAY && b.vx > 0);
    if (t >= aiNext) {  // reflejos: cada 320 ms (nivel 0) ... 90 ms (nivel 5). ko11.9.3: algo mas vivo
      aiNext = t + 320 - aiLevel * 46;
      float tx = homeX(s);
      if (state == VB_PLAY && mine) {
        float lx = predictX(head);
        // golpearla por la derecha (sale hacia el jugador), con el error de este punto
        if (lx > VB_NET_X) tx = lx + 12 + aiErr * (b.spiked ? 1.0f : 0.35f);
      }
      aiTarget = tx;
    }
    moveTo(s, aiTarget);
    if (state != VB_PLAY) return;
    float dx = b.x - me.x, dy = bodyY(s) - b.y;
    // saltar cuando baja cerca (mas a menudo cuanto mas nivel)
    if (!airborne(s) && b.vy > 0 && fabsf(dx) < 46 && dy > 70 && dy < 190 && b.x > VB_NET_X) {
      if (rng.below(100) < 6 + aiLevel * 5) jump(s);
    }
    // en el aire y cerca: remate a veces
    if (airborne(s) && !me.jumped && fabsf(dx) < 70 && fabsf(dy) < 80) {
      me.jumped = true;
      if (rng.below(100) < 12 + aiLevel * 9) spike(s);
    }
  }

  // Remate: velocidad horizontal pow hacia el otro campo y la caida justa para que
  // pase por encima de la red y bote en un punto al azar del otro lado.
  // false si desde aqui no se puede (muy bajo o pegado a la red por debajo).
  bool spikeAim(int s, float pow, float *vx, float *vy) {
    float dir = s ? -1.0f : 1.0f;
    float groundY = (float)VB_GROUND - VB_BALL_R, clearY = (float)VB_NET_TOP - VB_BALL_R - 6;
    float dn = ((float)VB_NET_X - b.x) * dir;  // distancia hasta la red (>0)
    if (dn < 1) dn = 1;
    float far = s ? (float)VB_LEFT + VB_BALL_R + 4 : (float)VB_RIGHT - VB_BALL_R - 4;
    float near = (float)VB_NET_X + dir * (VB_NET_HW + VB_BALL_R + 30);
    // la caida que llega al suelo en L, y la altura a la que pasa por la red
    auto vyFor = [&](float L) {
      float T = fabsf(L - b.x) / pow;
      return (groundY - b.y - 0.5f * VB_GRAV * T * T) / T;
    };
    auto clears = [&](float v) {
      float tn = dn / pow;
      return b.y + v * tn + 0.5f * VB_GRAV * tn * tn < clearY;
    };
    // el punto mas cercano a la red que aun pasa (pasos de 12 px)
    float L = near;
    while (!clears(vyFor(L)) && (far - L) * dir > 0) L += dir * 12;
    if ((far - L) * dir <= 0 || !clears(vyFor(L))) return false;
    float span = (far - L) * dir;
    L += dir * (float)rng.below((uint32_t)span + 1);
    float v = vyFor(L);
    if (v < 60) v = 60;  // siempre algo hacia abajo
    float vmax = sqrtf(VB_SPIKE_MAXV * VB_SPIKE_MAXV - pow * pow);  // no mas rapido que VB_SPIKE_MAXV
    if (v > vmax) v = vmax;
    *vx = dir * pow;
    *vy = v;
    return clears(v);
  }

  // ko11.9.1: el lado 0 se coloca solo donde bajara la pelota (reflejos buenos,
  // sin error): el jugador solo decide cuando saltar y rematar.
  void thinkAuto() {
    const int s = 0;
    if (t >= autoNext) {
      autoNext = t + 120;
      float tx = homeX(s);
      if (state == VB_PLAY && (b.x < VB_NET_X || b.vx < 0)) {
        float lx = predictX(bodyY(s) - p[s].hitR);
        if (lx < VB_NET_X) tx = lx - 20;  // por la izquierda: sale hacia el rival
      }
      autoTarget = tx;
    }
    moveTo(s, autoTarget);
  }

  // ---- un paso de la simulacion (dt en ms, se trocea en pasos de 10 ms) ----
  void step(uint32_t dtMs, bool aiOn = true) {
    while (dtMs > 0) {
      uint32_t d = dtMs > 10 ? 10 : dtMs;
      dtMs -= d;
      substep(d / 1000.0f, d);
      if (autoMove0) thinkAuto();
      if (aiOn) thinkAi();
    }
  }

  void substep(float dt, uint32_t dms) {
    t += dms;
    stateT += dms;
    for (int s = 0; s < 2; s++) {  // jugadores
      VbPlayer &q = p[s];
      if (q.moving) {
        float d = q.targetX - q.x, m = q.speed * dt;
        q.x += fabsf(d) <= m ? d : (d > 0 ? m : -m);
      }
      if (airborne(s)) {
        q.y += q.vy * dt;
        q.vy -= VB_PGRAV * dt;
        if (q.y <= 0) { q.y = 0; q.vy = 0; q.jumped = false; }
      }
    }
    if (state == VB_OVER) return;
    if (state == VB_POINT) {
      if (stateT >= VB_POINT_MS) {
        if (score[0] >= VB_WIN || score[1] >= VB_WIN) {
          state = VB_OVER;
          winner = score[1] > score[0];
          stateT = 0;
        } else {
          server = lastScorer;
          resetRally();
        }
      }
      return;
    }
    if (state == VB_SERVE) {  // la pelota espera quieta sobre quien saca
      if (stateT >= VB_SERVE_MS) { state = VB_PLAY; stateT = 0; }
      return;
    }
    // ---- pelota ----
    b.vy += VB_GRAV * dt;
    b.x += b.vx * dt;
    b.y += b.vy * dt;
    if (b.x - VB_BALL_R < VB_LEFT) { b.x = VB_LEFT + VB_BALL_R; b.vx = fabsf(b.vx) * 0.9f; }
    if (b.x + VB_BALL_R > VB_RIGHT) { b.x = VB_RIGHT - VB_BALL_R; b.vx = -fabsf(b.vx) * 0.9f; }
    if (b.y - VB_BALL_R < VB_CEIL) { b.y = VB_CEIL + VB_BALL_R; b.vy = fabsf(b.vy) * 0.6f; }
    // red
    if (fabsf(b.x - VB_NET_X) < VB_NET_HW + VB_BALL_R && b.y + VB_BALL_R > VB_NET_TOP) {
      if (b.y < VB_NET_TOP && b.vy > 0) {  // cae sobre el borde: rebota hacia arriba
        b.y = VB_NET_TOP - VB_BALL_R;
        b.vy = -fabsf(b.vy) * 0.75f;
      } else if (b.x < VB_NET_X) {
        b.x = VB_NET_X - VB_NET_HW - VB_BALL_R;
        b.vx = -fabsf(b.vx) * 0.7f;
      } else {
        b.x = VB_NET_X + VB_NET_HW + VB_BALL_R;
        b.vx = fabsf(b.vx) * 0.7f;
      }
    }
    // jugadores
    for (int s = 0; s < 2; s++) {
      VbPlayer &q = p[s];
      float cx = q.x, cy = bodyY(s);
      float dx = b.x - cx, dy = b.y - cy, rr = VB_BALL_R + q.hitR;
      float d2 = dx * dx + dy * dy;
      if (d2 >= rr * rr || t < q.coolUntil) continue;
      float d = sqrtf(d2);
      if (d < 0.01f) { dx = 0; dy = -1; d = 1; }
      float nx = dx / d, ny = dy / d;
      float dir = s ? -1.0f : 1.0f;
      float spVx = 0, spVy = 0;
      // ko11.9.1: con autoMove0 el jugador remata solo si toca la pelota en el aire
      bool wantSpike = q.spikeUntil > t || (s == 0 && autoMove0);
      bool canSpike = airborne(s) && wantSpike && ny < 0.6f && b.y < VB_NET_TOP - 30;
      if (canSpike) canSpike = spikeAim(s, q.spikePow, &spVx, &spVy);
      if (canSpike) {  // remate: fuerte y hacia abajo, cae dentro del otro campo
        b.vx = spVx;
        b.vy = spVy;
        b.spiked = true;
        b.spikeSide = (uint8_t)s;
        q.spikeUntil = 0;
        q.spikeT = t;
        spikes[s]++;
      } else if (b.spiked && b.spikeSide != s && rng.below(100) >= q.dig) {
        // remate mal recibido: la pelota rebota en el cuerpo hacia atras y cae en su campo
        b.vx = -dir * (90 + (float)rng.below(90));
        b.vy = 60;
        b.spiked = false;
        q.coolUntil = t + 900;
        hits[s]++;
        b.x = cx + nx * (rr + 1);
        b.y = cy + ny * (rr + 1);
        continue;
      } else {  // toque normal: siempre hacia arriba, empujando un poco al otro lado
        b.vx = nx * 330 + dir * 110;
        b.vy = ny * 420;
        if (b.vy > -360) b.vy = -360 - (float)rng.below(70);
        b.spiked = false;
      }
      b.x = cx + nx * (rr + 1);
      b.y = cy + ny * (rr + 1);
      float v = sqrtf(b.vx * b.vx + b.vy * b.vy);
      if (!b.spiked && v > VB_MAXV) { b.vx *= VB_MAXV / v; b.vy *= VB_MAXV / v; }
      q.coolUntil = t + 180;
      hits[s]++;
    }
    // suelo: punto para el otro lado
    if (b.y + VB_BALL_R >= VB_GROUND) {
      b.y = VB_GROUND - VB_BALL_R;
      lastScorer = b.x < VB_NET_X ? 1 : 0;
      score[lastScorer]++;
      state = VB_POINT;
      stateT = 0;
    }
  }
};
