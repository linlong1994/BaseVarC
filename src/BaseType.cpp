#include "BaseType.h"

BaseType::BaseType(BaseV base, BaseV qual, int ref, double minaf) : bases(base), quals(qual), ref_base(ref), min_af(minaf)
{
    nind = bases.size();
    var_qual = 0;
    init_allele_freq = new double[NTYPE]; // @WATCHOUT
    ind_allele_likelihood = new double[nind * NTYPE]; // @WATCHOUT
    for (int32_t i = 0; i < nind; ++i) {
        for (int j = 0; j < NTYPE; ++j) {
            if (bases[i] == BASE[j]) {
                ind_allele_likelihood[i * NTYPE + j] = 1.0 - exp(MLN10TO10 * quals[i]);
            } else {
                ind_allele_likelihood[i * NTYPE + j] = exp(MLN10TO10 * quals[i]) / 3.0;
            }
        }
        if (depth.count(bases[i]) == 1) depth[bases[i]] += 1;
    }
    for (DepM::iterator it = depth.begin(); it != depth.end(); ++it) {
        depth_total += it->second;
    }
}

void BaseType::combs(const BaseV& bases, CombV& comb_v, int32_t k)
{
    // k <= n
    int32_t n = bases.size();
    std::string bitmask(k, 1); // K leading 1's
    bitmask.resize(n, 0); // N-K trailing 0's

    BaseV bv;
    comb_v.clear();
    bv.reserve(k);
    do {
        for (int32_t i = 0; i < n; ++i) // [0..N-1] integers
        {
            if (bitmask[i]) bv.push_back(bases[i]);
        }
        comb_v.push_back(bv);
        bv.clear();
    } while (std::prev_permutation(bitmask.begin(), bitmask.end()));
}

void BaseType::SetAlleleFreq(const BaseV& bases)
{
    double depth_sum = 0;
    for (auto const& b : bases) {
        depth_sum += depth[b];
    }
    for (int j = 0; j < NTYPE; ++j) {
        init_allele_freq[j] = 0;
    }
    if (depth_sum > 0) {
        for (auto const& b : bases) {
            init_allele_freq[b] = depth[b] / depth_sum;
        }
    }
}

void BaseType::UpdateF(const BaseV& bases, CombV& bc, ProbV& lr, FreqV& bp, int32_t k)
{
    // REMINDME: consider comb_v.reserve
    combs(bases, bc, k);
    double *marginal_likelihood = new double[nind](); // @WATCHOUT
    double *expect_allele_prob = new double[NTYPE]();
    double freq_sum = 0;
    double likelihood_sum = 0;
    double epsilon = 0.001;
    int iter_num = 100;
    ProbV expect_prob;
    lr.clear();
    bp.clear();
    for (auto const& b: bc) {
        SetAlleleFreq(b);
        freq_sum = 0;
        for (int i = 0; i < NTYPE; ++i) freq_sum += init_allele_freq[i];
        if (freq_sum == 0) continue;  // skip coverage = 0, this may be redundant;
        // run EM
        EM(init_allele_freq, ind_allele_likelihood, marginal_likelihood, expect_allele_prob, nind, NTYPE, iter_num, epsilon);
        likelihood_sum = 0;
        for (int32_t i = 0; i < nind; ++i) {
            likelihood_sum += log(marginal_likelihood[i]);
            // reset array elements to 0
            marginal_likelihood[i] = 0;
        }
        expect_prob.assign(expect_allele_prob, expect_allele_prob + NTYPE);
        lr.push_back(likelihood_sum);
        bp.push_back(expect_prob);
        std::fill(expect_allele_prob, expect_allele_prob + NTYPE, 0);
    }

    delete []marginal_likelihood;
    delete []expect_allele_prob;
}

void BaseType::LRT()
{
    if (depth_total == 0) return;
    bases.clear();
    for (auto const& b : BASE) {
        // filter bases by count freqence >= min_af
        if (depth[b]/depth_total >= min_af) {
            bases.push_back(b);
        }
    }
    if (bases.size() == 0) return;
    CombV bc;
    FreqV bp;
    ProbV lr_null, lrt_chi, base_frq;
    UpdateF(bases, bc, lr_null, bp, bases.size());
    base_frq = bp[0];
    double lr_alt_t = lr_null[0];
    double chi_sqrt_t = 0;
    int i_min;
    for (int32_t k = bases.size() - 1; k > 0; --k) {
        UpdateF(bases, bc, lr_null, bp, k);
        lrt_chi.clear();
        for (auto lr_null_t: lr_null) {
            lrt_chi.push_back(2.0 * (lr_alt_t - lr_null_t));
        }
        i_min = std::min_element(lrt_chi.begin(), lrt_chi.end()) - lrt_chi.begin();
        lr_alt_t = lr_null[i_min];
        chi_sqrt_t = lrt_chi[i_min];
        if (chi_sqrt_t < LRT_THRESHOLD) {
            // Take the null hypothesis and continue
            bases = bc[i_min];
            base_frq = bp[i_min];
        } else {
            // Take the alternate hypothesis
            break;
        }
    }
    for (auto b: bases) {
        if (b != ref_base) {
            alt_bases.push_back(b);
            af_lrt.push_back(base_frq[b]);
        }
    }
    double r, chi_prob;
    if (alt_bases.size() > 0) {
        r = depth[bases[0]] / depth_total;
        if (bases.size() == 1 && depth_total > 10 && r > 0.5) {
            var_qual = 5000.0;  // mono-allelelic
        } else {
            chi_prob = chisf(chi_sqrt_t, 1.0);
            if (chi_prob) {
                var_qual = -10 * log10(chi_prob);
            } else {
                var_qual = 10000.0;
            }
        }
        if (var_qual == 0) {
            // output -0 to 0.0;
            var_qual = 0.0;
        }
    }
    return;
}

void BaseType::stats(int8_t ref_base, const BaseV& alt_bases, const AlleleInfoVector& aiv, Stat& s)
{
    ProbV ref_quals, ref_mapqs, ref_rprs;
    ProbV alt_quals, alt_mapqs, alt_rprs;
    s.ref_fwd = 0;
    s.ref_rev = 0;
    s.alt_fwd = 0;
    s.alt_rev = 0;
    for (auto const& ai: aiv) {
        if (ai.base == ref_base) {
            ref_quals.push_back(ai.qual);
            ref_mapqs.push_back(ai.mapq);
            ref_rprs.push_back(ai.rpr);
        } else if (std::find(alt_bases.begin(), alt_bases.end(), ai.base) != alt_bases.end()) {
            alt_quals.push_back(ai.qual);
            alt_mapqs.push_back(ai.mapq);
            alt_rprs.push_back(ai.rpr);
        }
        if (ai.strand == 1) {
            if (ai.base == ref_base) {
                s.ref_fwd += 1;
            } else if (std::find(alt_bases.begin(), alt_bases.end(), ai.base) != alt_bases.end()) {
                s.alt_fwd += 1;
            }
        } else if (ai.strand == 0) {
            if (ai.base == ref_base) {
                s.ref_rev += 1;
            } else if (std::find(alt_bases.begin(), alt_bases.end(), ai.base) != alt_bases.end()) {
                s.alt_rev += 1;
            }
        }
    }
    double z_qual = RankSumTest(ref_quals.data(), ref_quals.size(), alt_quals.data(), alt_quals.size());
    double z_mapq = RankSumTest(ref_mapqs.data(), ref_mapqs.size(), alt_mapqs.data(), alt_mapqs.size());
    double z_rpr  = RankSumTest(ref_rprs.data(), ref_rprs.size(), alt_rprs.data(), alt_rprs.size());
    s.phred_qual = -10 * log10(2 * normsf(abs(z_qual)));
    s.phred_mapq = -10 * log10(2 * normsf(abs(z_mapq)));
    s.phred_rpr  = -10 * log10(2 * normsf(abs(z_rpr)));
    if (isinf(s.phred_qual)) s.phred_qual = 10000.0;
    if (isinf(s.phred_mapq)) s.phred_mapq = 10000.0;
    if (isinf(s.phred_rpr)) s.phred_rpr = 10000.0;
    double left_p, right_p, twoside_p;
    kt_fisher_exact(s.ref_fwd, s.ref_rev, s.alt_fwd, s.alt_rev, &left_p, &right_p, &twoside_p);
    s.fs = -10 * log10(twoside_p);
    if (isinf(s.fs)) s.fs = 10000.0;
    if (s.ref_fwd * s.ref_rev > 0) {
        s.sor = static_cast<double>(s.ref_fwd * s.alt_fwd) / (s.ref_rev * s.alt_rev);
    } else {
        s.sor = 10000.0;
    }
}

void BaseType::writeVcf(const String& chr, int32_t pos, int8_t ref_base, const BaseType& bt, const AlleleInfoVector& aiv, const DepM& idx, int32_t N)
{
    std::unordered_map<uint8_t, String > alt_gt;
    String gt;
    for (size_t i = 0; i < bt.alt_bases.size(); ++i) {
        gt = "./" + std::to_string(i + 1);
        alt_gt.insert({bt.alt_bases[i], gt});
    }
    String sams = "";
    for (int32_t i = 0; i < N; ++i) {
        if (idx.count(i) == 0) {
            sams += "./.\t";
        } else {
            auto const& a = aiv[idx.at(i)];
            if (alt_gt.count(a.base) == 0) alt_gt.insert({a.base, "./."});
            if (a.base == ref_base) {
                gt = "0/.";
            } else {
                gt = alt_gt[a.base];
            }
            sams += gt + ":" + BASE2CHAR[a.base] + ":" + STRAND[a.strand] + ":" + std::to_string(1 - exp(MLN10TO10 * a.qual)) + "\t";
        }
    }
    Stat st;
    stats(ref_base, bt.alt_bases, aiv, st);
    double ad_sum = 0;
    String ac = "CM_AC=", af = "CM_AF=", caf = "CM_CAF=";
    std::stringstream alt_s;
    for (auto b : bt.alt_bases) {
        ad_sum += bt.depth.at(b);
        alt_s << BASE2CHAR[b] << ",";
        ac += std::to_string(bt.depth.at(b)) + ",";
        af += std::to_string(bt.af_lrt[b]) + ",";
        caf += std::to_string(bt.depth.at(b)/bt.depth_total) + ",";
    }
    String alt = alt_s.str();
    // remove the last char;
    ac.pop_back();
    af.pop_back();
    caf.pop_back();
    alt.pop_back();
    sams.pop_back();
    String qd = "QD=" + std::to_string(bt.var_qual/ad_sum);
    String dp = "CM_DP=" + std::to_string(static_cast<int>(bt.depth_total));
    String mq = "MQRankSum=" + std::to_string(st.phred_mapq);
    String rp = "ReadPosRankSum=" + std::to_string(st.phred_rpr);
    String bq = "BaseQRankSum=" + std::to_string(st.phred_qual);
    String fs = "FS=" + std::to_string(st.fs);
    String sor = "SOR=" + std::to_string(st.sor);
    String sb_ref = "SB_REF=" + std::to_string(st.ref_fwd) + "," + std::to_string(st.ref_rev);
    String sb_alt = "SB_ALT=" + std::to_string(st.alt_fwd) + "," + std::to_string(st.alt_rev);
    String qt;
    if (bt.var_qual > QUAL_THRESHOLD) {
        qt = ".";
    } else {
        qt = "LowQual";
    }
    char col = ';';
    char tab = '\t';
    std::stringstream out;
    out << chr << tab << pos << tab << '.' << tab << BASE2CHAR[ref_base] << tab << alt << tab << bt.var_qual << tab << qt << tab << bq << col << ac << col << af << col << caf << col << dp << col << mq << col << rp << col << sb_alt << col << sb_ref << col << sor << tab << sams;
    std::cout << out.str() << std::endl;
}