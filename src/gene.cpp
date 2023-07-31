//GAUSS: Genome Analysis Using Summary Statistics
//gene.cpp

#include "gene.h"

#include <iostream>
#include <vector>
#include <map>
#include <cmath>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_cdf.h>


#include "snp.h"
#include "gauss.h"
#include "util.h"
//#include "bgzf.h"

#define RunJepeg_Debug  //should move to gauss.h
#define CalJepegPval_Debug
#define GetW_Debug

Categ::Categ(int num, double pval, bool rmv){
  num_ = num;
  pval_ = pval;
  rmv_ = rmv;
}

std::string Categ::GetName(){
  std::string name;
  if(num_ == 0){
    name = "PFS";
  } else if(num_ == 1){
    name = "TFB";
  } else if(num_ == 2){
    name = "STR";
  } else if(num_ == 3){
    name = "TAR";
  } else if(num_ == 4){
    name = "CIS";
  } else if(num_ == 5){
    name = "TRN";
  }
  return name;
}

void Categ::PrintInfo(){
  std::cout<<num_<<" "<<pval_<<" "<<rmv_<<std::endl;
}

Gene::Gene(Arguments& args){
  
  //JEPEG/MIX output
  geneid_ = ".";
  chisq_ = -1.0;
  df_ = 0;
  jepeg_pval_ = -1.0;
  num_snp_ = 0;
  top_categ_name_ = ".";
  top_categ_pval_ = -1.0;
  top_snp_id_ = ".";
  top_snp_pval_ = -1.0;
  
  num_avail_categ_ = 0;
  num_rmv_categ_ = 0;
  
  num_protein_ = 0;
  num_tfbs_ = 0;
  num_wthhair_ = 0;
  num_wthtar_ = 0;
  num_cis_ = 0;
  num_trans_ = 0;  
  
  bonfe_pval_bf_ = -1.0;
  bonfe_pval_af_ = -1.0;
  sumU_pval_ = -1.0;
  
  lambda_ = args.lambda;
  min_abs_eig_ = args.min_abs_eig;
  total_num_categ_ = args.total_num_categ;
  categ_cor_cutoff_ = args.categ_cor_cutoff;
  denorm_norm_w_ = args.denorm_norm_w;
  
}


void Gene::RunJepeg(std::vector<Snp*>& gene_snp_vec){
  
  // count the number of SNPs belong to each categ.
  // TODO: make a function for the following
  std::vector<Snp*>::iterator it_gsv;
  for(it_gsv = gene_snp_vec.begin(); it_gsv != gene_snp_vec.end(); ++it_gsv){
    std::map< int, double >& categ_map = (*it_gsv)->GetCategMap();
    std::map< int, double >::iterator it_cm;
    for(it_cm = categ_map.begin(); it_cm != categ_map.end(); ++it_cm){
      if( (it_cm->first) == 0 ){       // Protein
        if(num_protein_ == 0)
          num_avail_categ_++;
        num_protein_++;
      } else if( (it_cm->first) == 1){ // TFBS
        if(num_tfbs_ == 0)
          num_avail_categ_++;
        num_tfbs_++;
      } else if( (it_cm->first) == 2){ // Within Hairpin
        if(num_wthhair_ == 0)
          num_avail_categ_++;
        num_wthhair_++;
      } else if( (it_cm->first) == 3){ // Within Target
        if(num_wthtar_ == 0)
          num_avail_categ_++;
        num_wthtar_++;
      } else if( (it_cm->first) == 4){ // cis eqtl
        if(num_cis_ == 0)
          num_avail_categ_++;
        num_cis_++;
      } else if( (it_cm->first) == 5){ // trans eqtl
        if(num_trans_ == 0)
          num_avail_categ_++;
        num_trans_++;
      }
    }	
  }//for

/*  
  num_snp_ = gene_snp_vec.size();
  
#ifdef RunJepeg_Debug
  std::cout<<std::endl;
  std::cout<<"############# num_snp_: " <<num_snp_<<std::endl;
#endif
  
  
  categ_count_vec_.push_back(num_protein_);
  categ_count_vec_.push_back(num_tfbs_);
  categ_count_vec_.push_back(num_wthhair_);
  categ_count_vec_.push_back(num_wthtar_);
  categ_count_vec_.push_back(num_cis_);
  categ_count_vec_.push_back(num_trans_);
  
  
#ifdef RunJepeg_Debug
  std::cout<<std::endl;
  std::cout<<"############# categ_count_vec_" << std::endl;
  PrintVector(categ_count_vec_);
#endif
  
  for(int i=0; i<categ_count_vec_.size(); i++){
    if(categ_count_vec_[i]){
      Categ categ(i, 0.0, false);
      categ_vec_.push_back(categ);
    }
  }
  
#ifdef RunJepeg_Debug
  std::cout<<std::endl;
  std::cout<<"############# categ_vec_" << std::endl;
  for(int i=0; i<num_avail_categ_; i++){
    categ_vec_[i].PrintInfo();
  }
#endif
  
  
  //Load genotypes of all SNPs in the gene_snp_vec.  
  for(std::vector<Snp*>::iterator it_gsv = gene_snp_vec.begin(); it_gsv != gene_snp_vec.end(); ++it_gsv){
    std::string genotype;
    bgzf_seek(fp_, (*it_gsv)->GetFpos(), SEEK_SET);
    BgzfGetLine(fp_, genotype);
    
    //check flip variable
    if((*it_gsv)->GetFlip()){ //if flip is on, flip the genotypes.
      FlipGenotype(genotype); //std::string& genotype
    }
    
    (*it_gsv)->SetGenotype(genotype);
  }
  
  
#ifdef RunJepeg_Debug
  std::cout<<std::endl;
  std::cout<<"############### gene_snp_vec (after loading genotypes)" <<std::endl;
  for(std::vector<Snp*>::iterator it_gsv=gene_snp_vec.begin(); it_gsv != gene_snp_vec.end(); ++it_gsv){
    //(*it_gsv)->PrintDistResult();
    (*it_gsv)->PrintSnpInfo();
  }
#endif
  
  //Calculate Jepeg p-value.
  CalJepegPval2(gene_snp_vec);    
  
  //Release genotypes of all snps in gene_snp_vec.
  for(std::vector<Snp*>::iterator it_gsv = gene_snp_vec.begin(); it_gsv != gene_snp_vec.end(); ++it_gsv){
    std::string& genotype = (*it_gsv)->GetGenotypeRef();
    std::string().swap(genotype); //release memory allocated for genotype
  }
  
#ifdef RunJepeg_Debug
  std::cout<<std::endl;
  std::cout<<"############### gene_snp_vec (after releasing genotypes)" <<std::endl;
  for(std::vector<Snp*>::iterator it_gsv=gene_snp_vec.begin(); it_gsv != gene_snp_vec.end(); ++it_gsv){
    //(*it_gsv)->PrintDistResult();
    (*it_gsv)->PrintSnpInfo();
  }
#endif
  
  */
}


double Gene::CalBonfePval(gsl_matrix* U){
  gsl_vector* pval_vec = gsl_vector_calloc(U->size1);
  double u, p;
  for(int i=0; i<U->size1; i++){
    u = gsl_matrix_get(U, i, 0);
    gsl_vector_set(pval_vec, i, 2*gsl_cdf_ugaussian_Q(std::abs(u))*(U->size1) );
  }
  p = gsl_vector_min(pval_vec);
  if(p > 1)
    p = 1.0;
  gsl_vector_free(pval_vec);
  return p;
}

double Gene::CalSumUPval(gsl_matrix* U, gsl_matrix* CovU){
  double sumU=0;
  double var_sumU=0;
  for(int i=0; i<CovU->size1; i++){
    sumU = sumU + gsl_matrix_get(U, i, 0);
    for(int j=0; j<CovU->size1; j++){
      var_sumU = var_sumU + gsl_matrix_get(CovU, i ,j);
    }
  }
  double p = 2*gsl_cdf_gaussian_Q(std::abs(sumU), std::sqrt(var_sumU));
  return p;
}
/*
 void Gene::CalJepegPval1(std::vector<Snp*>& gene_snp_vec){
 
 gsl_matrix* CorG = gsl_matrix_calloc(gene_snp_vec.size(), gene_snp_vec.size());
 gsl_matrix* W = gsl_matrix_calloc(num_categ_, gene_snp_vec.size()); // weight matrix
 gsl_matrix* Wt = gsl_matrix_calloc(gene_snp_vec.size(), num_categ_);
 gsl_matrix* WWt = gsl_matrix_calloc(num_categ_, num_categ_);
 gsl_matrix* W_CorG = gsl_matrix_calloc(num_categ_, gene_snp_vec.size());
 gsl_matrix* W_CorG_Wt = gsl_matrix_calloc(num_categ_, num_categ_); // covariance matrix of V
 gsl_matrix* CovU = gsl_matrix_calloc(num_categ_, num_categ_); // covariance matrix of U = correlation matrix of V
 
 gsl_matrix* Z = gsl_matrix_calloc(gene_snp_vec.size(), 1);
 gsl_matrix* V = gsl_matrix_calloc(num_categ_, 1); // V=W*Z
 gsl_matrix* U = gsl_matrix_calloc(num_categ_, 1); // normalized V
 
 //Get CorG (correlation matrix[n by n] btw snp genotypes)  
 for(size_t i=0; i<CorG->size1; i++){
 gsl_matrix_set(CorG, i, i, 1.0 + lambda_); //add LAMBDA here (ridge regression trick)
 for(size_t j=i+1; j<CorG->size1; j++){
 double v = CalCor((*gene_snp_vec[i]).GetGenotypeRef(), 
 (*gene_snp_vec[j]).GetGenotypeRef());
 //v = floor(v*DECIMAL+0.5)/DECIMAL;
 gsl_matrix_set(CorG, i, j, v);
 gsl_matrix_set(CorG, j, i, v);
 }
 }
#ifdef CalJepegPval_Debug
 std::cout<<"############# CorG" << std::endl;
 PrintMatrix(CorG);
#endif
 
 //Get W (weight score matrix[k by n])
 GetW(W, gene_snp_vec);
#ifdef CalJepegPval_Debug
 std::cout<<"############# W" << std::endl;
 PrintMatrix(W);
#endif
 
 //Get Wt
 gsl_matrix_transpose_memcpy(Wt, W);
#ifdef CalJepegPval_Debug
 std::cout<<"############# Wt" << std::endl;
 PrintMatrix(Wt);
#endif 
 
 //Get WWt
 MpMatMat(WWt, W, Wt);
#ifdef CalJepegPval_Debug
 std::cout<<"############# WWt" << std::endl;
 PrintMatrix(WWt);
#endif 
 
 //Get Cov V  
 MpMatMat(W_CorG, W, CorG);
 MpMatMat(W_CorG_Wt, W_CorG, Wt); //covariance matrix of V
#ifdef CalJepegPval_Debug
 std::cout<<"############# W_CorG_Wt (Cov V)" << std::endl;
 PrintMatrix(W_CorG_Wt);
#endif 
 
 //Get Cov U (= Cor U = Cor V) 
 CnvrtCovToCor(CovU, W_CorG_Wt); // covariance (correlation) matrix of U = correlation matrix of V
#ifdef CalJepegPval_Debug
 std::cout<<"############# CovU (=CorU=CorV)" << std::endl;
 PrintMatrix(CovU);
#endif 
 
 //Get Z
 GetZ(Z, gene_snp_vec);
#ifdef CalJepegPval_Debug
 std::cout<<"############# Z" << std::endl;
 PrintMatrix(Z);
#endif 
 
 //Get V
 MpMatMat(V, W, Z);
#ifdef CalJepegPval_Debug
 std::cout<<"############# V" << std::endl;
 PrintMatrix(V);
#endif 
 
 //Get U
 for(int i=0; i< U->size1; i++){
 double var = gsl_matrix_get(W_CorG_Wt, i, i);
 double u = gsl_matrix_get(V,i,0)/std::sqrt(var);
 gsl_matrix_set(U,i,0,u);
 }
#ifdef CalJepegPval_Debug
 std::cout<<"############# U" << std::endl;
 PrintMatrix(U);
#endif 
 
 //Make and init rmv_categ_list
 //This vector will be used in excluding collinear/low varinace categs from U and CovU.
 std::vector<bool> rmv_categ_list;
 for(int i=0; i<num_categ_; i++){
 rmv_categ_list.push_back(false);
 }
#ifdef CalJepegPval_Debug
 std::cout<<"############# rmv_categ_list (init)" << std::endl;
 PrintVector(rmv_categ_list);
#endif   
 
 //TODO: find collinear categs (to be removed).
 for(int j=num_categ_-1; j>0; j--){
 for(int i=0; i<j; i++){
 double cor = gsl_matrix_get(CovU, i, j);
 if(std::abs(cor) > categ_cor_cutoff_){
 rmv_categ_list[j]=true;
 break;
 }
 }
 }
#ifdef CalJepegPval_Debug
 std::cout<<"############# rmv_categ_list (collinear)" << std::endl;
 PrintVector(rmv_categ_list);
#endif   
 
 //TODO: find categs with low variance (to be removed).  
 for(int i=0; i<num_categ_; i++){
 double sd_WZ = gsl_matrix_get(W_CorG_Wt, i, i); 
 double norm_W = gsl_matrix_get(WWt, i, i)/denorm_norm_w_;
 if(sd_WZ < norm_W){
 rmv_categ_list[i]=true;
 }
 }
#ifdef CalJepegPval_Debug
 std::cout<<"############# rmv_categ_list (low var)" << std::endl;
 PrintVector(rmv_categ_list);
#endif   
 
 //TODO: count the number of categs to be removed.
 for(int i=0; i<num_categ_; i++){
 if(rmv_categ_list[i])
 num_rmv_categ_++;
 }
#ifdef CalJepegPval_Debug
 std::cout<<"############# num_categ_ and num_rmv_categ_" << std::endl;
 std::cout<<"num_categ_: "<<num_categ_<<std::endl;
 std::cout<<"num_rmv_categ_: "<<num_rmv_categ_<<std::endl<<std::endl;
#endif   
 
 
 
 if(num_categ_ > num_rmv_categ_){
 
 gsl_matrix* X = gsl_matrix_calloc(num_categ_ - num_rmv_categ_ , 1); 
 gsl_matrix* Xt = gsl_matrix_calloc(1, num_categ_ - num_rmv_categ_);
 gsl_matrix* CovX = gsl_matrix_calloc(num_categ_ - num_rmv_categ_, num_categ_ - num_rmv_categ_);
 gsl_matrix* CovXInv = gsl_matrix_calloc(num_categ_ - num_rmv_categ_, num_categ_ - num_rmv_categ_);
 gsl_matrix* Xt_CovXInv = gsl_matrix_calloc(1, num_categ_ - num_rmv_categ_);
 gsl_matrix* Xt_CovXInv_X = gsl_matrix_calloc(1, 1); //chisq
 
 //Remove rmv categs from U.
 //Make new vector X.
 int ii=0;
 for(int i=0; i<num_categ_; i++){
 rmv_categ_list_debug_.push_back(rmv_categ_list[i]); //for Debugging
 U_list_debug_.push_back(gsl_matrix_get(U, i, 0)); //for Debugging
 if(!rmv_categ_list[i]){
 gsl_matrix_set(X, ii, 0, gsl_matrix_get(U, i, 0));
 ii++;
 }
 }
#ifdef CalJepegPval_Debug
 std::cout<<"############# X" << std::endl;
 PrintMatrix(X);
#endif 
 
 //Remove rmv categs from CovU.
 //Make new matrix CovX.
 int n = 0;
 int m = 0;
 for(int i=0; i<num_categ_; i++){
 if(!rmv_categ_list[i]){
 for(int j=0; j<num_categ_; j++){
 if(!rmv_categ_list[j]){
 gsl_matrix_set(CovX, n, m, gsl_matrix_get(CovU, i, j));
 m++;
 }
 }
 m=0;
 n++;
 }
 }
#ifdef CalJepegPval_Debug
 std::cout<<"############# CovX" << std::endl;
 PrintMatrix(CovX);
#endif 
 
 
 //Get Xt
 gsl_matrix_transpose_memcpy(Xt, X);
#ifdef CalJepegPval_Debug
 std::cout<<"############# Xt" << std::endl;
 PrintMatrix(Xt);
#endif 
 
 //Get inverce matrix of CovX (CovXInv)
 MakePosDef(CovX, min_abs_eig_);
 InvMat(CovXInv, CovX);
#ifdef CalJepegPval_Debug
 std::cout<<"############# CovXInv" << std::endl;
 PrintMatrix(CovXInv);
#endif 
 
 //Get Chisq value (Xt * CovXInv * X)
 MpMatMat(Xt_CovXInv, Xt, CovXInv);
 MpMatMat(Xt_CovXInv_X, Xt_CovXInv, X);
#ifdef CalJepegPval_Debug
 std::cout<<"############# Xt_CovXInv_X (chisq value)" << std::endl;
 PrintMatrix(Xt_CovXInv_X);
#endif 
 
 chisq_ = gsl_matrix_get(Xt_CovXInv_X, 0, 0);
 jepeg_pval_ = gsl_cdf_chisq_Q(chisq_, num_categ_- num_rmv_categ_);
 
 bonfe_pval_bf_ = CalBonfePval(U);
 bonfe_pval_af_ = CalBonfePval(X);
 sumU_pval_ = CalSumUPval(X, CovX);
 
#ifdef CalJepegPval_Debug
 std::cout<<"############# JEPEG results" << std::endl;
 std::cout<<"chisq_: "<<chisq_<<std::endl;
 std::cout<<"jepeg_pval_: "<<jepeg_pval_<<std::endl;
 std::cout<<"bonfe_pval_bf_: "<<bonfe_pval_bf_<<std::endl;
 std::cout<<"bonfe_pval_af_: "<<bonfe_pval_af_<<std::endl;
 std::cout<<"sumU_pval_: "<<sumU_pval_<<std::endl<<std::endl;
#endif   
 
 
 gsl_matrix_free(X);
 gsl_matrix_free(Xt);
 gsl_matrix_free(CovX);
 gsl_matrix_free(CovXInv);
 gsl_matrix_free(Xt_CovXInv);  
 gsl_matrix_free(Xt_CovXInv_X);
 }
 
 gsl_matrix_free(CorG);
 gsl_matrix_free(W);
 gsl_matrix_free(Wt);
 gsl_matrix_free(WWt);
 gsl_matrix_free(W_CorG);
 gsl_matrix_free(W_CorG_Wt);
 gsl_matrix_free(CovU);
 
 gsl_matrix_free(Z);
 gsl_matrix_free(V);
 gsl_matrix_free(U);
 
 }
 */

void Gene::CalJepegPval2(std::vector<Snp*>& gene_snp_vec){
  
  gsl_matrix* CorG = gsl_matrix_calloc(gene_snp_vec.size(), gene_snp_vec.size()); // =CorZ
  gsl_matrix* W = gsl_matrix_calloc(num_avail_categ_, gene_snp_vec.size()); // weight matrix
  gsl_matrix* Wt = gsl_matrix_calloc(gene_snp_vec.size(), num_avail_categ_);
  gsl_matrix* WWt = gsl_matrix_calloc(num_avail_categ_, num_avail_categ_);
  gsl_matrix* W_CorG = gsl_matrix_calloc(num_avail_categ_, gene_snp_vec.size());
  gsl_matrix* CovU = gsl_matrix_calloc(num_avail_categ_, num_avail_categ_); // CovU (covariance matrix of U)= W * CorG * Wt
  //To remove collinear categs
  gsl_matrix* CorU = gsl_matrix_calloc(num_avail_categ_, num_avail_categ_); // CorU (correlation matrix of U)
  
  gsl_matrix* Z = gsl_matrix_calloc(gene_snp_vec.size(), 1);
  gsl_matrix* U = gsl_matrix_calloc(num_avail_categ_, 1); 
  gsl_matrix* Ut = gsl_matrix_calloc(1, num_avail_categ_); 
  gsl_matrix* normU = gsl_matrix_calloc(num_avail_categ_, 1); //normalized U
  
  
  //Get CorG (correlation matrix[n by n] btw snp genotypes)  
  for(size_t i=0; i<CorG->size1; i++){
    gsl_matrix_set(CorG, i, i, 1.0 + lambda_); //add LAMBDA here (ridge regression trick)
    for(size_t j=i+1; j<CorG->size1; j++){
      double v = CalCor((*gene_snp_vec[i]).GetGenotypeVec(), 
                        (*gene_snp_vec[j]).GetGenotypeVec());
      //v = floor(v*DECIMAL+0.5)/DECIMAL;
      gsl_matrix_set(CorG, i, j, v);
      gsl_matrix_set(CorG, j, i, v);
    }
  }
#ifdef CalJepegPval_Debug
  std::cout<<"############# CorG" << std::endl;
  PrintMatrix(CorG);
#endif
  
  //Get W (weight score matrix[k by n])
  GetW(W, gene_snp_vec);
#ifdef CalJepegPval_Debug
  std::cout<<"############# W" << std::endl;
  PrintMatrix(W);
#endif
  
  //Get Wt
  gsl_matrix_transpose_memcpy(Wt, W);
#ifdef CalJepegPval_Debug
  std::cout<<"############# Wt" << std::endl;
  PrintMatrix(Wt);
#endif 
  
  //Get WWt
  MpMatMat(WWt, W, Wt);
#ifdef CalJepegPval_Debug
  std::cout<<"############# WWt" << std::endl;
  PrintMatrix(WWt);
#endif 
  
  //Get Cov U  
  MpMatMat(W_CorG, W, CorG);
  MpMatMat(CovU, W_CorG, Wt); //covariance matrix of U
#ifdef CalJepegPval_Debug
  std::cout<<"############# CovU (W*CorG*Wt)" << std::endl;
  PrintMatrix(CovU);
#endif 
  
  //Get Cor U 
  CnvrtCovToCor(CorU, CovU); // covariance (correlation) matrix of U = correlation matrix of V
#ifdef CalJepegPval_Debug
  std::cout<<"############# CorU" << std::endl;
  PrintMatrix(CorU);
#endif 
  
  //Get Z
  GetZ(Z, gene_snp_vec);
#ifdef CalJepegPval_Debug
  std::cout<<"############# Z" << std::endl;
  PrintMatrix(Z);
#endif 
  
  //Get U
  MpMatMat(U, W, Z);
#ifdef CalJepegPval_Debug
  std::cout<<"############# U (=W*Z)" << std::endl;
  PrintMatrix(U);
#endif 
  
  //Get normU (normalized U)
  for(int i=0; i< normU->size1; i++){
    double var = gsl_matrix_get(CovU, i, i);
    double u = gsl_matrix_get(U,i,0)/std::sqrt(var);
    gsl_matrix_set(normU,i,0,u);
    categ_vec_[i].SetPval(2*gsl_cdf_ugaussian_Q(std::abs(u)));
  }
#ifdef CalJepegPval_Debug
  std::cout<<"############# normU (nomalized U) = U/sqrt(varU)" << std::endl;
  PrintMatrix(normU);
#endif 
  
#ifdef CalJepegPval_Debug
  std::cout<<"############# Print Categ Vector before removing" << std::endl;
  for(int i=0; i<num_avail_categ_; i++){
    categ_vec_[i].PrintInfo();
  }
#endif   
  
  //TODO: find collinear categs (to be removed).
  for(int j=num_avail_categ_-1; j>0; j--){
    for(int i=0; i<j; i++){
      double cor = gsl_matrix_get(CorU, i, j);
      if(std::abs(cor) > categ_cor_cutoff_){
        categ_vec_[j].SetRmv(true);
        break;
      }
    }
  }
#ifdef CalJepegPval_Debug
  std::cout<<"############# Print Categ Vector after removing collinear categs" << std::endl;
  for(int i=0; i<num_avail_categ_; i++){
    categ_vec_[i].PrintInfo();
  }
#endif   
  
  //TODO: find categs with low variance (to be removed).  
  for(int i=0; i<num_avail_categ_; i++){
    double varU = gsl_matrix_get(CovU, i, i); 
    double normW = gsl_matrix_get(WWt, i, i)/denorm_norm_w_;
    if(varU < normW){
      categ_vec_[i].SetRmv(true);
    }
  }
#ifdef CalJepegPval_Debug
  std::cout<<"############# Print Categ Vector after removing categs with low variance" << std::endl;
  for(int i=0; i<num_avail_categ_; i++){
    categ_vec_[i].PrintInfo();
  }
  //PrintVector(rmv_categ_list);
#endif   
  
  //The number of available categs after removing collinear or low-variance categs
  for(int i=0; i<num_avail_categ_; i++){
    if(categ_vec_[i].GetRmv())
      num_rmv_categ_++;
  }
  df_ = num_avail_categ_ - num_rmv_categ_;
  
#ifdef CalJepegPval_Debug
  std::cout<<"############# The number of available/rmv categs after removing collinear or low-variance categs" << std::endl;
  std::cout<<"num_avail_categ_: "<<num_avail_categ_<<std::endl;
  std::cout<<"num_rmv_categ_: "<<num_rmv_categ_<<std::endl;
  std::cout<<"df_: "<<df_<<std::endl;
#endif   
  
  //////////////////////////////////////////////////////////////////////////////////// 
  //START
  //1. make new U (X) and CovU (CovX) after removing collinear or low variance categs
  //2. calculate JEPEG pvalue 
  if(df_){
    gsl_matrix* X = gsl_matrix_calloc(df_ , 1);
    gsl_matrix* Xt = gsl_matrix_calloc(1, df_);
    gsl_matrix* CovX = gsl_matrix_calloc(df_, df_);
    gsl_matrix* CovXInv = gsl_matrix_calloc(df_, df_);
    gsl_matrix* Xt_CovXInv = gsl_matrix_calloc(1, df_);
    gsl_matrix* Xt_CovXInv_X = gsl_matrix_calloc(1, 1); //chisq
    
    //Remove rmv categs from U.
    //Make new vector X.
    int ii=0;
    for(int i=0; i<num_avail_categ_; i++){
      if(!(categ_vec_[i].GetRmv())){
        gsl_matrix_set(X, ii, 0, gsl_matrix_get(U, i, 0));
        ii++;
      }
    }
    
#ifdef CalJepegPval_Debug
    std::cout<<"############# X" << std::endl;
    PrintMatrix(X);
#endif
    
    //Remove rmv categs from CovU.
    //Make new matrix CovX.
    int n = 0;
    int m = 0;
    for(int i=0; i<num_avail_categ_; i++){
      if(!(categ_vec_[i].GetRmv())){
        for(int j=0; j<num_avail_categ_; j++){
          if(!(categ_vec_[j].GetRmv())){
            gsl_matrix_set(CovX, n, m, gsl_matrix_get(CovU, i, j));
            m++;
          }
        }
        m=0;
        n++;
      }
    }
#ifdef CalJepegPval_Debug
    std::cout<<"############# CovX" << std::endl;
    PrintMatrix(CovX);
#endif
    
    //Get Xt
    gsl_matrix_transpose_memcpy(Xt, X);
#ifdef CalJepegPval_Debug
    std::cout<<"############# Xt" << std::endl;
    PrintMatrix(Xt);
#endif
    
    //Get inverce matrix of CovX (CovXInv)
    MakePosDef(CovX, min_abs_eig_);
    InvMat(CovXInv, CovX);
#ifdef CalJepegPval_Debug
    std::cout<<"############# CovXInv" << std::endl;
    PrintMatrix(CovXInv);
#endif 
    
    //Get JEPEG Chisq value (Xt * CovXInv * X)
    MpMatMat(Xt_CovXInv, Xt, CovXInv);
    MpMatMat(Xt_CovXInv_X, Xt_CovXInv, X);
#ifdef CalJepegPval_Debug
    std::cout<<"############# JEPEG Chisq value = Xt * CovXInv * X" << std::endl;
    PrintMatrix(Xt_CovXInv_X);
#endif 
    
    chisq_ = gsl_matrix_get(Xt_CovXInv_X, 0, 0);
    jepeg_pval_ = gsl_cdf_chisq_Q(chisq_, df_);
    
    //bonfe_pval_bf_ = CalBonfePval(U);
    //bonfe_pval_af_ = CalBonfePval(X);
    //sumU_pval_ = CalSumUPval(X, CovX);
    
    //////////////////////////////
    Categ top_categ = GetTopCateg(categ_vec_);
    top_categ_name_ = top_categ.GetName();
    top_categ_pval_ = top_categ.GetPval();
    
    Snp* top_snp = GetTopSNP(gene_snp_vec);
    top_snp_id_ = (*top_snp).GetRsid();
    top_snp_pval_ = 2*gsl_cdf_ugaussian_Q(std::abs((*top_snp).GetZ()));
    
#ifdef CalJepegPval_Debug
    std::cout<<"############# JEPEG results" << std::endl;
    std::cout<<"chisq_: "<<chisq_<<std::endl;
    std::cout<<"df_: "<< df_ <<std::endl;
    std::cout<<"jepeg_pval_: "<<jepeg_pval_<<std::endl;
    
    std::cout<<"top_categ_name_: "<<top_categ_name_<<std::endl;
    std::cout<<"top_categ_pval_: "<<top_categ_pval_<<std::endl; // Z-score
    
    std::cout<<"top_snp_id_: "<<top_snp_id_<<std::endl;
    std::cout<<"top_snp_pval_: "<<top_snp_pval_<<std::endl;
    //std::cout<<"top_snp_pval_: "<<top_snp_pval_<<std::endl;
    
    //std::cout<<"bonfe_pval_bf_: "<<bonfe_pval_bf_<<std::endl;
    //std::cout<<"bonfe_pval_af_: "<<bonfe_pval_af_<<std::endl;
    //std::cout<<"sumU_pval_: "<<sumU_pval_<<std::endl<<std::endl;
#endif   
    
    gsl_matrix_free(X);
    gsl_matrix_free(Xt);
    gsl_matrix_free(CovX);
    gsl_matrix_free(CovXInv);
    gsl_matrix_free(Xt_CovXInv);
    gsl_matrix_free(Xt_CovXInv_X);
    
    //END 
    //1. make new U (X) and CovU (CovX) after removing collinear or low variance categs
    //2. calculate JEPEG pvalue 
    ////////////////////////////////////////////////////////////////////////////////////
  }
  
  gsl_matrix_free(CorG);
  gsl_matrix_free(W);
  gsl_matrix_free(Wt);
  gsl_matrix_free(WWt);
  gsl_matrix_free(W_CorG);
  gsl_matrix_free(CovU);
  gsl_matrix_free(CorU);
  
  gsl_matrix_free(Z);
  gsl_matrix_free(U);
  gsl_matrix_free(Ut);
  gsl_matrix_free(normU);
}


void Gene::GetZ(gsl_matrix* Z, std::vector<Snp*>& gene_snp_vec){
  for(int i=0; i<gene_snp_vec.size(); i++){
    gsl_matrix_set(Z, i, 0, (*gene_snp_vec[i]).GetZ());
  }
}


void Gene::GetW(gsl_matrix* W, std::vector<Snp*>& gene_snp_vec){
  
#ifdef GetW_Debug
  std::cout<<"############# categ_count_vec_" << std::endl;
  std::cout<<"size: "<<categ_count_vec_.size()<< std::endl;
  PrintVector(categ_count_vec_);
#endif  
  for(int i=0; i<gene_snp_vec.size(); i++){
    int k=0;
    for(int j=0; j<categ_count_vec_.size(); j++){
      if(categ_count_vec_[j]!=0){
        gsl_matrix_set(W, k, i, ((*gene_snp_vec[i]).GetCategWgt(j))*std::sqrt(((*gene_snp_vec[i]).GetInfo())) );
        //gsl_matrix_set(W, k, i, ((*gene_snp_vec[i]).GetCategWgt(j)));
        k++;
      }
    }
  }
}

/*
 void Gene::GetW2(gsl_matrix* W, std::vector<Snp*>& gene_snp_vec){
 
#ifdef GetW_Debug
 std::cout<<"############# categ_vec_" << std::endl;
 PrintVector(categ_vec_);
#endif  
 
 for(int i=0; i<gene_snp_vec.size(); i++){
 for(int j=0; j<categ_vec_.size(); j++){
 gsl_matrix_set(W, j, i, ((*gene_snp_vec[i]).GetCategWgt(j))*std::sqrt(((*gene_snp_vec[i]).GetInfo())) );
 //gsl_matrix_set(W, j, i, ((*gene_snp_vec[i]).GetCategWgt(j)));
 }
 }
 
 }
 */




Categ Gene::GetTopCateg(std::vector<Categ>& categ_vec){
  int top_index = 0;
  for(int i=0; i<num_avail_categ_; i++){
    double top_p = categ_vec[top_index].GetPval();
    double p = categ_vec[i].GetPval();
    bool rmv = categ_vec[i].GetRmv();
    if( (top_p > p) & !rmv ){
      top_index = i;
    }
  }
  return categ_vec[top_index];
}

//TODO: need to select Top SNP from only non-collinear categs
Snp* Gene::GetTopSNP(std::vector<Snp*>& gene_snp_vec){
  int top_index = 0;
  for(int i=0; i<gene_snp_vec.size(); i++){
    double top_absz = std::abs((*gene_snp_vec[top_index]).GetZ());
    double absz = std::abs((*gene_snp_vec[i]).GetZ());
    if(top_absz < absz){
      top_index = i;
    }
  }
  return gene_snp_vec[top_index];
}
