#include <cassert>
#include <iostream>
#include <string>
#include <cmath>
#include <limits>
#include <ctime>
#include <algorithm>

#include "sampler.h"
#include "str_util.h"
#include "model.h"

void Model::run(string doc_pt, string res_dir, string doc_user) {
  load_docs(doc_pt, doc_user);
  cout<<"after load doc"<<endl;
  model_init();

  cout << "Begin iteration" << endl;
  string out_dir = res_dir + "k" + str_util::itos(K) + ".";
  for (int it = 1; it < n_iter + 1; ++it) {
	cout << "\riter " << it << '/' << n_iter;
	fflush(stdout);
    /**
	for (int b = 0; b < bs.size(); ++b) {
	  update_biterm(bs[b]);
	}
    **/
    for (int d = 0; d < blogs.size(); ++d) {
      update_docs(d);
    }
	
	if (it % save_step == 0)
	  save_res(out_dir);
  }

  save_res(out_dir);
}

void Model::model_init() {
  srand(time(NULL));
  // random initialize
  nu_z.resize(UN+1, K);
  ndz.resize(blogs.size(), K);
  /**
  for (vector<Biterm>::iterator b = bs.begin(); b != bs.end(); ++b) {
	int k = Sampler::uni_sample(K);
	assign_biterm_topic(*b, k);
  }
  **/
  for (int i = 0; i < blogs.size(); ++i) {
    vector<Biterm> bts = blogs[i];
    for (int j = 0; j < bts.size(); ++j) {
      int k = Sampler::uni_sample(K);
      assign_biterm_topic(bts[j], k, i);
    }
  }
}

// input, each line is a doc
// format: wid  wid  wid ...
void Model::load_docs(string dfile, string doc_user, string htfile) {
  cout << "load docs: " << dfile << endl;
  ifstream rf( dfile.c_str() );
  if (!rf) {
	cout << "file not find:" << dfile << endl;
	exit(-1);
  }

  ifstream userf(doc_user.c_str());
  if (!userf) {
    cout << "file not find:" << doc_user << endl;
    exit(-1);
  }

  ifstream htf(htfile.cstr());
  if(!htf) {
      cout << "htflie not find" << endl;
      exit(-1);
  }

  string line;
  string line_user;
  string line_ht;
  int user;
  while(getline(rf, line)) {
    getline(userf, line_user);
    istringstream useriss(line_user);
    useriss >> user;
    if (user>UN) { 
        UN = user;
    }

    getline(htf, line_ht);

	Doc doc(line, line_ht, user);
	doc.gen_biterms();
    blogs.push_back(doc);
	for (int i = 0; i < doc.size(); ++i) {
	  int w = doc.get_w(i);
	  pw_b[w] += 1;
	}
  }
  
  pw_b.normalize();
}

void Model::update_docs(int d) {
    dc = blogs[d];
    vector<Biterm> bts= dc.get_bts();
    for(int i = 0; i < bts.size(); i++) {
        reset_biterm_topic(bts[i], d);
        Pvec<double> pz;
        compute_pz_b(bts[i], pz, d);
        int k = Sampler::mult_sample(pz.to_vector());
        assign_biterm_topic(bts[i], k);
    }

// sample procedure for ith biterm 
void Model::update_biterm(Biterm& bi) {
  reset_biterm_topic(bi);
  
  // compute p(z|b)
  Pvec<double> pz;
  compute_pz_b(bi, pz);

  // sample topic for biterm b
  int k = Sampler::mult_sample(pz.to_vector());
  assign_biterm_topic(bi, k);
}

// reset topic assignment of biterm i, biterm i belong to blog d
void Model::reset_biterm_topic(Biterm& bi, int d) {
  int k = bi.get_z();
  // not is the background topic
  int w1 = bi.get_wi();
  int w2 = bi.get_wj();
  int u = bi.get_user();
 
  nb_z[k] -= 1;	// update number of biterms in topic K
  nwz[k][w1] -= 1;	// update w1's occurrence times in topic K
  nwz[k][w2] -= 1;
  nu_z[u][k] -= 1;
  ndz[d][k] -= 1;
  assert(nb_z[k] > -10e-7 && nwz[k][w1] > -10e-7 && nwz[k][w2] > -10e-7);
  bi.reset_z();
}

// compute p(z|w_i, w_j)
void Model::compute_pz_b(Biterm& bi, Pvec<double>& pz, int d) {
  pz.resize(K);
  int w1 = bi.get_wi();
  int w2 = bi.get_wj();
  int user = bi.get_user();
  int ub_num = 0;   //当前用户所有双词数
  for(int k = 0; k < K; ++k) {
    ub_num += nu_z[user][k];
  }
  
  double pw1k, pw2k, pk, puk;
  for (int k = 0; k < K; ++k) {
	// avoid numerical problem by mutipling W
	if (has_background && k == 0) {
	  pw1k = pw_b[w1];
	  pw2k = pw_b[w2];
	}
	else {
	  pw1k = (nwz[k][w1] + beta) / (2 * nb_z[k] + W * beta);
	  pw2k = (nwz[k][w2] + beta) / (2 * nb_z[k] + 1 + W * beta);
	}
    puk = (nu_z[user][k] + sigma) / (ub_num + K * alpha);
	//pk = (nb_z[k] + alpha) / (bs.size() + K * alpha);
	pk = (ndz[k] + 1) / (ndz[k] + sigma);
	pz[k] = pk * pw1k * pw2k * puk;
  }

  //pz.normalize();
}

// assign topic k to biterm i, biterm i belong to blog d
void Model::assign_biterm_topic(Biterm& bi, int k, int d) {
  bi.set_z(k);
  int w1 = bi.get_wi();
  int w2 = bi.get_wj();
  int u = bi.get_user();
  nb_z[k] += 1;
  nwz[k][w1] += 1;
  nwz[k][w2] += 1;
  nu_z[u][k] += 1;
  ndz[d][k] += 1;
}


void Model::save_res(string dir) {
  string pt = dir + "pz";
  cout << "\nwrite p(z): " << pt << endl;
  save_pz(pt);
  
  string pt2 = dir + "pw_z";
  cout << "write p(w|z): " << pt2 << endl;
  save_pw_z(pt2);

  string pt3 = dir + "pu_z";
  cout << "write p(u|z): " << pt3 << endl;
  save_pu_z(pt3);
}

// p(z) is determinated by the overall proportions
// of biterms in it
void Model::save_pz(string pt) {
  Pvec<double> pz(nb_z);
  pz.normalize(alpha);
  pz.write(pt);
}

void Model::save_pw_z(string pt) {
  Pmat<double> pw_z(K, W);   // p(w|z) = phi, size K * M
  ofstream wf(pt.c_str());
  for (int k = 0; k < K; k++) {
	for (int w = 0; w < W; w++) 
	  pw_z[k][w] = (nwz[k][w] + beta) / (nb_z[k] * 2 + W * beta);

	wf << pw_z[k].str() << endl;
  }
}
void Model::save_pu_z(string dir){
  Pmat<double> pu_z(UN+1,K);
  ofstream wf(dir.c_str());
  for (int i = 0; i < UN + 1; i++) {
        //cout<<"sum "<<sum<<endl;
         for (int j = 0; j < K; j++) {
                  pu_z[i][j] = (nu_z[i][j] + sigma ) / ( nb_z[j] + UN*sigma );
          }
          wf << pu_z[i].str() << endl;
  }
}
