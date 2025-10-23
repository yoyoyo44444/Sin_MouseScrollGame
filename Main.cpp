# include <Siv3D.hpp> // Siv3D v0.6.16

struct Player
{
	Vec2 pos; // 位置
	Vec2 velocity; // 速度

	double radius = 25.0;

	void update(double wheel, double mouseX, double lagTimer)
	{
		// y移動（スクロール）
		const double accelY = wheel * 1.3; // 加速度
		velocity.y += accelY;

		velocity.y *= 0.911; // 摩擦係数
		pos.y += velocity.y;

		// x移動（カーソル）
		pos.x = (lagTimer <= 0.0) ? Math::Lerp(pos.x, mouseX, 0.01) : pos.x;


		// 浮力
		const double centerY = (Scene::Center().y - 400.0);
		pos.y = Math::Lerp(pos.y, centerY, 0.007);
	}

	void draw() const
	{
		// プレイヤー描画（仮）
		Circle{ pos, radius }.draw(Palette::Orange);
	}
};


struct Obstacle
{
	Vec2 pos;
	double radius;
	bool isAlive = true;
	double explosionTime = 0.0; // 爆発アニメーション用タイマー

	void update(double scrollVel)
	{
		pos.y -= scrollVel; // スクロール速度で移動

		if ( (Scene::Height() + radius) < pos.y ) // 画面外に出たら
		{
			isAlive = false;
		}

		if (0.0 < explosionTime)
		{
			explosionTime += Scene::DeltaTime();
			if (0.4 < explosionTime)
			{
				isAlive = false;
			}
		}
	}

	void explode()
	{
		explosionTime = 0.001; // 爆発開始
	}

	void draw() const
	{
		if (0.0 < explosionTime) // 爆発アニメーション
		{
			double size = radius + explosionTime * 80;
			Circle{ pos, size }.drawFrame(6, HSV{ Random(0.0, 360.0), 0.7, 1.0 });
		}
		else
		{
			Circle{ pos, radius }.draw(ColorF{ 0.6});
		}
	}
};


struct Bubble
{
	Vec2 pos;
	double radius;
	double speed;
	double life; // 泡の生存時間

	Bubble(Vec2 start)
	{
		pos = start + Vec2{ Random(-8.0, 8.0), Random(- 8.0, 8.0) };
		radius = Random(2.0, 6.0);
		speed = Random(60.0, 100.0);
		life = 1.5;
	}

	bool update(double scrollVelocity)
	{
		pos.y -= ( speed + scrollVelocity * 15) * Scene::DeltaTime();
		life -= Scene::DeltaTime() * 0.7;
		return (0.0 < life);
	}

	void draw() const
	{
		ColorF color{ 0.8, 0.9, 1.0, life * 0.1 };
		Circle{ pos, radius }.draw(color);
	}
};


void Main()
{
	Window::Resize(800, 1300);

	Scene::SetBackground(ColorF{ 0.1, 0.15, 0.2 });

	Player player{ Vec2{ Scene::Center() } };

	double scrollY = 0.0;
	double scrollVelocity = 0.0;
	double scrollAccel = 0.0;

	double lagTimer = 0.0; // 被弾時のラグ

	Array<Obstacle> obstacles;

	Array<Bubble> bubbles;

	// タイムアタックモード
	double totalDepth = 10000.0; // ゴールまでの距離
	double currentDepth = 0.0;
	Stopwatch timer{ StartImmediately::Yes }; // タイム計測

	const Font uiFont{ FontMethod::MSDF, 28, Typeface::Medium };


	// 背景テクスチャ作成（仮）
	const int texHeight = 1300;
	Image gradient(1, texHeight);

	for (int y = 0; y < texHeight; ++y)
	{
		double hue = (y * 360.0 / texHeight);
		gradient[y][0] = HSV{ hue, 0.6, 0.5 };
	}

	const Texture gradTex{ gradient, TextureDesc::Mipped };

	const double tileHeight = texHeight;

	bool gameClear = false;



	while (System::Update())
	{
		if (gameClear)
		{
			uiFont(U"GOAL!").drawAt(Scene::Center(), ColorF{ 1.0, 0.9, 0.3 });
			uiFont(U"Time: {:.0f} sec"_fmt(timer.sF())).drawAt(Scene::Center().x, (Scene::Center().y + 80));

			if (MouseL.down())
			{
				// リスタート
				gameClear = false;
				timer.restart();
				currentDepth = 0.0;
				scrollY = scrollVelocity = 0.0;
				obstacles.clear();
			}
			continue;
		}


		if (0.0 < lagTimer)
		{
			lagTimer -= Scene::DeltaTime();
		}


		// ホイール入力を取得
		const double wheel = (lagTimer <= 0.0) ? Mouse::Wheel() : 0.0;

		// プレイヤー更新
		player.update(wheel, Cursor::Pos().x, lagTimer);


		// 背景スクロールの加速・減速
		scrollAccel = wheel * 2.0; // 加速度
		scrollVelocity += scrollAccel;
		scrollVelocity *= 0.97; // 摩擦係数
		scrollY += scrollVelocity;

		// 深度計算
		currentDepth += scrollVelocity * 0.25;
		currentDepth = Clamp(currentDepth, 0.0, totalDepth);

		// ゴール判定
		if (totalDepth <= currentDepth)
		{
			gameClear = true;
			timer.pause();
		}


		// 背景描画
		double offsetY = Math::Fmod(scrollY, tileHeight);
		if (offsetY < 0) offsetY += tileHeight;

		// 画面を埋めるために2枚描画
		gradTex.resized(Scene::Width(), tileHeight).draw(0, -offsetY);
		gradTex.resized(Scene::Width(), tileHeight).draw(0, -offsetY + tileHeight);


		// 泡の生成
		double speedAbs = Abs(scrollVelocity); // スクロールスピードの絶対値
		if (1.0 < speedAbs)
		{
			// スクロール速度に応じて泡の量を増やす
			int bubbleCount = static_cast<int>(Min(speedAbs * 0.1, 5.0));
			for (int i = 0; i < bubbleCount; ++i)
			{
				bubbles << Bubble{ player.pos };
			}
		}

		for (auto& b : bubbles)
		{
			b.update(scrollVelocity);
			b.draw();
		}

		bubbles.remove_if([](const Bubble& b) { return b.life <= 0.0; });



		// 障害物
		if (RandomBool(0.2))
		{
			double x = Random(50.0, Scene::Width() - 50.0);
			obstacles << Obstacle{ Vec2{ x, 1350 }, 30, true };
		}

		for (auto& ob : obstacles)
		{
			ob.update(scrollVelocity);
			ob.draw();

			if (ob.isAlive && ob.explosionTime == 0.0 && Circle{ player.pos, player.radius }.intersects(Circle{ ob.pos, ob.radius }))
			{
				ob.explode();

				lagTimer = 1.5;

				scrollVelocity *= 0.3; // 慣性演出
			}
		}

		obstacles.remove_if([](const Obstacle& ob) { return !ob.isAlive; });


		// プレイヤー描画
		player.draw();

		// マップ
		const double mapX = Scene::Width() - 60;
		const double mapTop = 100;
		const double mapBottom = Scene::Height() - 100;
		const double mapHeight = mapBottom - mapTop;

		RectF{ mapX, mapTop, 20, mapHeight }.draw(ColorF{ 0.1, 0.4, 0.6, 0.3 });
		double markerY = mapTop + (currentDepth / totalDepth) * mapHeight;
		RectF{ mapX, markerY - 5, 20, 10 }.draw(ColorF{ 1.0, 0.9, 0.3 });

		// UI表示
		uiFont(Format(static_cast<int>(currentDepth), U" m")).draw(20, (mapX - 5), (markerY - 35), ColorF{ 0.9 });
		uiFont(U"Time: {:.2f} sec"_fmt(timer.sF())).draw(20, 60, ColorF{ 0.9 });
	}
}
