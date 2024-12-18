package uwebfilter.categorify

import io.gatling.core.Predef._
import io.gatling.http.Predef._
import scala.concurrent.duration._

class CategorifySimulation extends Simulation {

  val httpProtocol = http
    .baseUrl("http://localhost:9005")
    .acceptHeader("application/json")
    .contentTypeHeader("multipart/form-data")

  val modelName = "mymodel"
  val modelCSV  = "dataset.csv"
  val feeder = csv("text.csv.gz").unzip.random

  val scn = scenario("ML Train-Inference-Delete")
    .exec(
       http("Model: Train")
        .post("/train")
        .formParam("name", modelName)
        .formUpload("csvfile", modelCSV)
        .check(status.is(201))
    )
    .pause(5)

    .repeat(100) {
      feed(feeder)
        .exec(
          http("Model: Inference")
            .post("/inference")
            .header("Content-Type", "application/json")
            .body(StringBody( s"""{"model": "${modelName}", "text": "#{text}"}""" )).asJson
            .check(status.is(200))
        )
        .pause(250.milliseconds, 500.milliseconds)
    }
    .pause(5)

    .exec(
      http("Model: Delete")
        .delete("/models")
        .header("Content-Type", "application/json")
        .body(StringBody( s"""{"name": "$modelName"}""" )).asJson
        .check(status.is(200))
    )

  setUp(
    scn.inject(atOnceUsers(1))
  ).protocols(httpProtocol)

}
