#include "hack.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

int twitch_in_fd = -1;
int twitch_out_fd = -1;
char twitch_buf[1024];
int twitch_buf_size;
char infile[SAVESIZE + 3];
char outfile[SAVESIZE + 4];

staticfn void process_twitch_cmd(char*);
staticfn boolean has_out_file(void);
staticfn void failed_effect(char*);
staticfn void successful_effect(char*);
staticfn void grant_object(char*);

void open_twitch(void) {
  snprintf(infile, SAVESIZE + 3, "%s.in", gs.SAVEF);
  snprintf(outfile, SAVESIZE + 4, "%s.out", gs.SAVEF);

  twitch_in_fd = open(infile, O_RDONLY | O_NONBLOCK);
  if (twitch_in_fd >= 0) {
    twitch_out_fd = open(outfile, O_WRONLY | O_NONBLOCK);
  }
}

void check_twitch(void) {
  if (twitch_in_fd < 0) {
    return;
  }

  int size;
  size = read(twitch_in_fd, &twitch_buf[twitch_buf_size], 1024 - twitch_buf_size);

  if (size < 0) {
    if (errno == EAGAIN) {
      return;
    }

    return;
  }

  if (size > 0) {
    twitch_buf_size += size;

    int i;
    boolean done;
    done = FALSE;

    while (!done) {
      done = TRUE;
      for (i = 0; i < twitch_buf_size; i++) {
        if (twitch_buf[i] == '\r' || twitch_buf[i] == '\n') {
          twitch_buf[i] = '\0';
          if (i > 0) {
            process_twitch_cmd(twitch_buf);
          }
          int j;
          for (j = 0; j + i + 1 < twitch_buf_size; j++) {
            twitch_buf[j] = twitch_buf[j + i + 1];
          }
          twitch_buf_size -= i + 1;
          done = FALSE;
          break;
        }
      }
    }
  }
}

staticfn void process_twitch_cmd(char *cmd) {
  char *id = strtok(cmd, " ");
  char *effect = strtok(NULL, " ");

  if (effect == NULL) {
    return;
  }

  if (!strcmp(effect, "create_monster")) {
    create_critters(1, NULL, FALSE);
    successful_effect(id);
  } else if (!strcmp(effect, "identify")) {
    identify_pack(1, TRUE);
    successful_effect(id);
  } else if (!strcmp(effect, "teleport")) {
    tele();
    successful_effect(id);
  } else if (!strcmp(effect, "level_teleport")) {
    if (u.uhave.amulet || In_endgame(&u.uz) || In_sokoban(&u.uz)) {
      failed_effect(id);
    } else {
      level_tele();
      successful_effect(id);
    }
  } else if (!strcmp(effect, "healing")) {
    You_feel("better.");
    healup(8 + d(4, 4), 0, FALSE, FALSE);
    successful_effect(id);
  } else if (!strcmp(effect, "extra_healing")) {
    You_feel("much better.");
    healup(16 + d(4, 8), 0, FALSE, FALSE);
    successful_effect(id);
  } else if (!strcmp(effect, "full_healing")) {
    You_feel("completely healed.");
    healup(400, 0, FALSE, FALSE);
    successful_effect(id);
  } else if (!strcmp(effect, "curse_inventory")) {
    if (!Blind) {
      You("notice a %s glow surrounding you.", hcolor(NH_BLACK));
    }
    rndcurse();
    successful_effect(id);
  } else if (!strcmp(effect, "reduce_luck")) {
    You_feel("unlucky.");
    change_luck(-1);
    successful_effect(id);
  } else if (!strcmp(effect, "fix_trouble")) {
    int trouble = in_trouble();
    if (trouble != 0) {
      fix_worst_trouble(trouble);
      successful_effect(id);
    } else {
      failed_effect(id);
    }
  } else if (!strcmp(effect, "darkness")) {
    litroom(FALSE, NULL);
    successful_effect(id);
  } else if (!strcmp(effect, "confuse")) {
    if (!Confusion) {
      if (Hallucination) {
          pline("What a trippy feeling!");
      } else {
          pline("Huh, What?  Where am I?");
      }
    }

    make_confused(itimeout_incr(HConfusion, rn1(7, 16)), FALSE);
    successful_effect(id);
  } else if (!strcmp(effect, "hallucinate")) {
    (void) make_hallucinated(itimeout_incr(HHallucination, d(10, 12)), TRUE, 0L);
    successful_effect(id);
  } else if (!strcmp(effect, "stun")) {
    make_stunned(itimeout_incr(HStun, rn1(10, 10)), TRUE);
    successful_effect(id);
  } else if (!strcmp(effect, "blind")) {
    make_blinded(itimeout_incr(BlindedTimeout, d(10, 12)), (boolean) !Blind);
    successful_effect(id);
  } else if (!strcmp(effect, "sleep")) {
    if (Sleep_resistance) {
      You("yawn.");
      failed_effect(id);
    } else {
      You("suddenly fall asleep!");
      fall_asleep(-rn1(10, 13), TRUE);
      successful_effect(id);
    }
  } else if (!strcmp(effect, "grant_object")) {
    char *object = strtok(NULL, "\n");
    grant_object(object);
    successful_effect(id);
  }
}

staticfn void grant_object(char *objname) {
  struct obj *otmp;
  otmp = readobjnam(objname, NULL);
  if (!otmp) {
    otmp = readobjnam((char *) 0, (struct obj *) 0);
  } else if (otmp == &hands_obj) {
    return;
  }

  if (otmp->oartifact) {
    pline("For a moment, you feel %s in your %s, but it disappears!",
        something, makeplural(body_part(HAND)));
    return;
  }

  const char *verb = ((Is_airlevel(&u.uz) || u.uinwater) ? "slip" : "drop"),
             *oops_msg = (u.uswallow
                          ? "Oops!  %s out of your reach!"
                          : (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz)
                             || levl[u.ux][u.uy].typ < IRONBARS
                             || levl[u.ux][u.uy].typ >= ICE)
                             ? "Oops!  %s away from you!"
                             : "Oops!  %s to the floor!");

  (void) hold_another_object(otmp, oops_msg, The(aobjnam(otmp, verb)), (const char *) 0);
}

staticfn boolean has_out_file(void) {
  if (twitch_out_fd >= 0) {
    return TRUE;
  }

  twitch_out_fd = open(outfile, O_WRONLY | O_NONBLOCK);

  return twitch_out_fd >= 0;
}

staticfn void failed_effect(char *id) {
  if (has_out_file()) {
    write(twitch_out_fd, "failure ", 8);
    write(twitch_out_fd, id, strlen(id));
    write(twitch_out_fd, "\n", 1);
  }
}

staticfn void successful_effect(char *id) {
  if (has_out_file()) {
    write(twitch_out_fd, "success ", 8);
    write(twitch_out_fd, id, strlen(id));
    write(twitch_out_fd, "\n", 1);
  }
}
