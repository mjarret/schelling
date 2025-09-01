// ... keep existing GnuplotLive ...

    // ---- NEW: variable-width histogram: using 1:xmid, 2:count, 3:width ----
    void plot_histogram_variable(const std::vector<double>& xmid,
                                 const std::vector<double>& count,
                                 const std::vector<double>& w,
                                 const std::string& title)
    {
        if (!valid_) return;
        cmd("set style fill solid 0.8");
        // variable box width from column 3
        std::fprintf(gp_, "plot '-' using 1:2:3 with boxes lc rgb '#4E79A7' title '%s'\n",
                     escape(title).c_str());
        const std::size_t m = std::min({xmid.size(), count.size(), w.size()});
        for (std::size_t i=0;i<m;++i)
            std::fprintf(gp_, "%.*g %.*g %.*g\n", 16, xmid[i], 16, count[i], 16, w[i]);
        std::fprintf(gp_, "e\n");
        std::fflush(gp_);
    }
