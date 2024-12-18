package uwebfilter.server

import io.gatling.core.Predef._
import io.gatling.http.Predef._

import scala.concurrent.duration._


class QueryDomainSimulation extends Simulation {

  val httpProtocol = http
    .baseUrl("http://localhost:3500")
    .header("Content-Type", "application/json")


  val feeder = csv("domains.csv.gz").unzip.random

  val scn1 = scenario("Post Query Scenario 1")
    .feed(feeder)
    .exec(
      http("Post Query Request")
        .post("/query")
        .body(StringBody( """{"domain": "#{domain}"}""" )).asJson
        .check(status.is(200))
        // .check(jsonPath("$.category").is("0"))
    )

  val scn2 = scenario("Post Query Scenario 2")
    .repeat(1000) {
      feed(feeder)
        .exec(
          http("Post Query Request")
            .post("/query")
            .body(StringBody( """{"domain": "#{domain}"}""" )).asJson
            .check(status.is(200))
        )
        .pause(100.milliseconds, 500.milliseconds)
    }


  setUp(
    scn1.inject(
      atOnceUsers(10000)
    ),
    scn2.inject(
      rampUsers(1000).during(10)
    )
  ).protocols(httpProtocol)

}
