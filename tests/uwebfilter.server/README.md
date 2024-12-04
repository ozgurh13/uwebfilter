
load testing uwebfilter.server using Gatling

`src/test/resources/domains.csv.gz` is the "Top 10 million websites" list

steps:
  1. sh docker-run.sh
  2. cd /root/uwebfilter.server
  3. sh setup.sh
  4. sh run.sh
