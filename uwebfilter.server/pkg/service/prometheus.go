package service

import (
	"github.com/prometheus/client_golang/prometheus"
)

type PrometheusMetrics struct {
	IPAddrs *prometheus.CounterVec
}

var (
	prometheusMetrics PrometheusMetrics = PrometheusMetrics{
		IPAddrs: prometheus.NewCounterVec(prometheus.CounterOpts{
			Namespace: "uwebfilter_server",
			Name:      "ipaddrs",
			Help:      "Number of requests made per ipaddr",
		}, []string{"ipaddr"}),
	}
)

func init() {
	prometheus.MustRegister(prometheusMetrics.IPAddrs)
}

func PrometheusIncIPAddrCount(ipaddr string) {
	prometheusMetrics.IPAddrs.With(
		prometheus.Labels{"ipaddr": ipaddr},
	).Inc()
}
