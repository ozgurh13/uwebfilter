import pickle
import pandas as pd


class Model:

    __slots__ = (
        "model",
        "id_to_category",
        "fitted_vectorizer",
        "df_category_unique",
    )

    def __init__(
        self,
        model=None,
        id_to_category=None,
        fitted_vectorizer=None,
        df_category_unique=None,
    ):
        self.model = model
        self.id_to_category = id_to_category
        self.fitted_vectorizer = fitted_vectorizer
        self.df_category_unique = df_category_unique

    def classify(self, text):
        """
        given a piece of text, return the classification
        """
        model = self.model
        transformed = self.fitted_vectorizer.transform([text])
        prediction = self.id_to_category[model.predict(transformed)[0]]

        data = pd.DataFrame(
            model.predict_proba(transformed) * 100,
            columns=self.df_category_unique,
        ).T

        data.columns = ["Probability"]
        data.index.name = "Category"

        probability = data.sort_values(
            ["Probability"],
            ascending=False,
        ).apply(lambda x: round(x, 2))

        return prediction, probability

    @staticmethod
    def load(modelfile):
        with open(modelfile, "rb") as handle:
            utils = pickle.load(handle)

        return Model(**utils)


if __name__ == "__main__":
    import argparse

    cmdline = argparse.ArgumentParser(
        description="classify text given a model",
    )

    cmdline.add_argument("--model", required=True, help="the model to use")

    words = cmdline.add_mutually_exclusive_group(required=True)
    words.add_argument(
        "--text",
        type=str,
        help="the text to classify",
    )
    words.add_argument(
        "--file",
        type=str,
        help="the file containing the text to classify",
    )

    cmdline.add_argument(
        "--probs",
        action="store_true",
        help="show probabilities",
    )

    args = cmdline.parse_args()

    model = Model.load(args.model)
    text = args.text
    if text is None:
        with open(args.file) as f:
            text = f.read()

    print(model.classify(text)[args.probs])
