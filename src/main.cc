#include "env.hh"
#include "hgaprec.hh"
#include "ratings.hh"

#include <stdlib.h>
#include <string>
#include <sstream>
#include <signal.h>

string Env::prefix = "";
Logger::Level Env::level = Logger::DEBUG;
FILE *Env::_plogf = NULL;
void usage();
void test();

Env *env_global = NULL;
volatile sig_atomic_t sig_handler_active = 0;

void
term_handler(int sig)
{
  if (env_global) {
    printf("Got signal. Saving model state.\n");
    fflush(stdout);
    env_global->save_state_now = 1;
  } else {
    signal(sig, SIG_DFL);
    raise(sig);
  }
}

int
main(int argc, char **argv)
{
  signal(SIGTERM, term_handler);
  if (argc <= 1) {
    printf("gaprec -dir <netflix-dataset-dir> -n <users>" \
	   "-m <movies> -k <dims> -label <out-dir-tag>\n");
    exit(0);
  }

  string fname;
  uint32_t n = 0, m = 0;
  uint32_t k = 0;
  string ground_truth_fname;
  uint32_t rfreq = 10;
  string label;
  bool logl = false;
  uint32_t max_iterations = 1000;
  bool nmi = false;
  bool strid = false;
  double rand_seed = 0;

  bool test = false;
  bool batch = true;
  bool online = false;
  bool gen_heldout = false;

  bool model_load = false;
  string model_location = "";

  bool hol_load = false;
  string hol_location = "";
  bool vb = true;
  
  bool pred_accuracy = false;
  bool gt_accuracy = false;
  bool p = false;
  double a = 0.3, b = 0.3, c = 0.3, d = 0.3;
  Env::Dataset dataset = Env::MENDELEY;
  bool binary_data = false;
  bool bias = false;
  bool hier = false;
  bool explore = false;
  bool gen_ranking_for_users = false;

  uint32_t i = 0;
  while (i <= argc - 1) {
    if (strcmp(argv[i], "-dir") == 0) {
      fname = string(argv[++i]);
      fprintf(stdout, "+ dir = %s\n", fname.c_str());
    } else if (strcmp(argv[i], "-n") == 0) {
      n = atoi(argv[++i]);
      fprintf(stdout, "+ n = %d\n", n);
    } else if (strcmp(argv[i], "-p") == 0) {
      p = true;
    } else if (strcmp(argv[i], "-m") == 0) {
      m = atoi(argv[++i]);
      fprintf(stdout, "+ m = %d\n", m);
    } else if (strcmp(argv[i], "-k") == 0) {
      k = atoi(argv[++i]);
      fprintf(stdout, "+ k = %d\n", k);
    } else if (strcmp(argv[i], "-nmi") == 0) {
      ground_truth_fname = string(argv[++i]);
      fprintf(stdout, "+ ground truth fname = %s\n",
	      ground_truth_fname.c_str());
      nmi = true;
    } else if (strcmp(argv[i], "-rfreq") == 0) {
      rfreq = atoi(argv[++i]);
      fprintf(stdout, "+ rfreq = %d\n", rfreq);
    } else if (strcmp(argv[i], "-strid") == 0) {
      strid = true;
      fprintf(stdout, "+ strid mode\n");
    } else if (strcmp(argv[i], "-label") == 0) {
      label = string(argv[++i]);
    } else if (strcmp(argv[i], "-logl") == 0) {
      logl = true;
      fprintf(stdout, "+ logl mode\n");
    } else if (strcmp(argv[i], "-max-iterations") == 0) {
      max_iterations = atoi(argv[++i]);
      fprintf(stdout, "+ max iterations %d\n", max_iterations);
    } else if (strcmp(argv[i], "-seed") == 0) {
      rand_seed = atof(argv[++i]);
      fprintf(stdout, "+ random seed set to %.5f\n", rand_seed);
    } else if (strcmp(argv[i], "-load") == 0) {
      model_load = true;
      model_location = string(argv[++i]);
      fprintf(stdout, "+ loading theta from %s\n", model_location.c_str());
    } else if (strcmp(argv[i], "-test") == 0) {
      test = true;
      fprintf(stdout, "+ test mode\n");
    } else if (strcmp(argv[i], "-batch") == 0) {
      batch = true;
      fprintf(stdout, "+ batch inference\n");
    } else if (strcmp(argv[i], "-online") == 0) {
      batch = false;
      fprintf(stdout, "+ online inference\n");
    } else if (strcmp(argv[i], "-gen-heldout") == 0) {
      gen_heldout = true;
      fprintf(stdout, "+ generate held-out files from dataset\n");
    } else if (strcmp(argv[i], "-pred-accuracy") == 0) {
      pred_accuracy = true;
      fprintf(stdout, "+ compute predictive accuracy\n");
    } else if (strcmp(argv[i], "-gt-accuracy") == 0) {
      gt_accuracy = true;
      fprintf(stdout, "+ compute  accuracy to ground truth\n");
    } else if (strcmp(argv[i], "-netflix") == 0) {
      dataset = Env::NETFLIX;
    } else if (strcmp(argv[i], "-mendeley") == 0) {
      dataset = Env::MENDELEY;
    } else if (strcmp(argv[i], "-movielens") == 0) {
      dataset = Env::MOVIELENS;
    } else if (strcmp(argv[i], "-echonest") == 0) {
      dataset = Env::ECHONEST;
    } else if (strcmp(argv[i], "-nyt") == 0) {
      dataset = Env::NYT;
    } else if (strcmp(argv[i], "-a") == 0) {
      a = atof(argv[++i]);
    } else if (strcmp(argv[i], "-b") == 0) {
      b = atof(argv[++i]);
    } else if (strcmp(argv[i], "-c") == 0) {
      c = atof(argv[++i]);
    } else if (strcmp(argv[i], "-d") == 0) {
      d = atof(argv[++i]);
    } else if (strcmp(argv[i], "-binary-data") == 0) {
      binary_data = true;
    } else if (strcmp(argv[i], "-bias") == 0) {
      bias = true;
    } else if (strcmp(argv[i], "-hier") == 0) {
      hier = true;
    } else if (strcmp(argv[i], "-gen-ranking") == 0) {
      gen_ranking_for_users = true;
    } else if (strcmp(argv[i], "-novb") == 0) {
      vb = false;
    } else if (i > 0) {
      fprintf(stdout,  "error: unknown option %s\n", argv[i]);
      assert(0);
    } 
    ++i;
  };

  Env env(n, m, k, fname, nmi, ground_truth_fname, rfreq, 
	  strid, label, logl, rand_seed, max_iterations, 
	  model_load, model_location, 
	  gen_heldout, a, b, c, d, dataset, 
	  batch, binary_data, bias, hier, explore, vb);
  env_global = &env;

  Ratings ratings(env);
  if (ratings.read(fname.c_str()) < 0) {
    fprintf(stderr, "error reading dataset from dir %s; quitting\n", 
	    fname.c_str());
    return -1;
  }

  if (gen_ranking_for_users) {
    HGAPRec hgaprec(env, ratings);
    hgaprec.gen_ranking_for_users();
    exit(0);
  }

  if (batch) {
    HGAPRec hgaprec(env, ratings);
    if (bias)
      hgaprec.vb_bias();
    else if (hier)
      hgaprec.vb_hier();
    else
      hgaprec.vb();
  } else {
    printf("Quitting. Online inference not implemented.\n");
    fflush(stdout);
    exit(0);
  }
}
