

Monitoring with docker
======================

This explains the procedure to start exporting metrics to a prometheus instance and optionally read them on Grafana.
It is Linux oriented, but being centered on Docker, it should work as well on other platforms.

# Prerequisites
Docker & docker compose are installed. Then place yourself in this project root directory.

Create a `monitoring/data/grafana` directory if it does not exist. It will contain `Grafana` data:
```
mkdir -p monitoring/data/grafana
```

# Launch
In this README we will simply expose the steps using docker. If you wish to install (or have already installed) Prometheus & Grafana by yourself you can refer to the options used in the `monitoring/docker-compose.yml` file which are a good starting point to connect the services to `coincenter`.

Prometheus service will read `monitoring/prometheus.yml` config file at start.

```
# Make sure images are up to date
docker-compose -f monitoring/docker-compose.yml pull

# Bring up the Prometheus with Grafana service in detached mode
docker-compose -f monitoring/docker-compose.yml up -d cct_grafana
```

At this step, we are ready to receive logs : [http://localhost:9090/](http://localhost:9090/).

# Stop
Use Ctrl-C (in attached mode), or :

```
docker-compose -f monitoring/docker-compose.yml stop
```
If you do a `docker-compose down`, **you will lose all your data**.

# Usage
The push gateway allows you to send metrics through HTTP calls. Example :
```
echo  "example_fake_calls{result=\"success\"} 25" | curl --data-binary @- http://localhost:9091/metrics/job/example_job
```

# Grafana
Grafana will be available [here](http://localhost:3000). It allows you to have persistent graphs & alerting.
The first time you launch grafana, you should access it with admin/admin credentials.

# Naming convention

[Link to prometheus guideline](https://prometheus.io/docs/practices/naming/)

## Metric name
- Single word
- Single unit
- Suffix describes unit
- Same logic behind what is being mesured

*Good examples :*
- process_cpu_seconds_total
- foobar_build_info

## Labels
Use labels to differentiate the characteristics of the thing that is being measured:
currency="EOS" or "USD"...
